/*******************************************************************************
 * File Name.c
 *
 * Created on: May 27, 2026
 * Author Christian dos Santos
 *
 ******************************************************************************/

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "dwin_update.h"
#include "../DWIN_functions/dwin_service.h"
#include "zigbee_app_framework_event.h"
#include <string.h>

#include "dwin_update_file_handler.h"
#include "dwin_update_flash.h"
/*******************************************************************************
 * Data types
 ******************************************************************************/
dwin_update_t update;
static sl_zigbee_event_t dwin_update_event;
static bool update_event_initialized = false;

static uint8_t dwin_test_ram[DWIN_TEST_RAM_SIZE];
static uint8_t dwin_test_flash[DWIN_TEST_FLASH_SIZE];
/*******************************************************************************
 * Extern
 ******************************************************************************/

/*******************************************************************************
 * Private Function Prototypes
 ******************************************************************************/
static sl_status_t load_next_chunk(void);
static sl_status_t send_buffer_to_ram(void);
static void dwin_update_ram_write_ack_callback(sl_status_t status, uint16_t vp, void *context);
static sl_status_t dwin_update_write_ram_packet(uint16_t ram_address, size_t packet_size, const uint8_t *data);
static sl_status_t fill_ram_with_zero(void);
static uint32_t dwin_update_get_block_size(void);

static sl_status_t dwin_update_flash_write_with_retry(void);
static sl_status_t dwin_test_flash_write_block(uint16_t flash_block, uint16_t ram_address, uint16_t delay_ms);
static sl_status_t dwin_os_update_block(uint16_t flash_block,
                                        uint16_t ram_address,
                                        uint16_t delay_ms);
static void init_update_event(void);
static void update_event_handler(sl_zigbee_event_t *event);
static void dwin_update_case_enable_crc(sl_status_t *status);
static void dwin_update_case_wait_crc_enables(sl_status_t *status);
static void dwin_update_case_load_block(sl_status_t *status);
static void dwin_update_case_fill_block(sl_status_t *status);
static void dwin_update_case_write_ram(sl_status_t *status);
static void dwin_update_case_flash_write(sl_status_t *status);
static void dwin_update_case_wait_flash(sl_status_t *status);
static void dwin_update_case_verify_block(sl_status_t *status);
static void dwin_update_case_next_block(sl_status_t *status);
static void dwin_update_case_update_finish(sl_status_t *status);
static void dwin_update_case_error(sl_status_t *status);
//static void dwin_update_confirm_retry_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context);
//static void dwin_update_cancel_retry_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context);
static sl_status_t dwin_update_request_flash_status_with_retry(void);
static bool dwin_update_is_recoverable_error(sl_status_t status);

static sl_status_t dwin_test_write_ram(uint16_t ram_address, size_t size, const uint8_t *data);
static bool dwin_test_verify_flash_block(uint32_t block_index);
static void dwin_update_restart_current_block(void);
static void dwin_update_debug_print(void);
/*******************************************************************************
 * Function name:
 *
 * Description:
 * Parameteres:
 * Returns:
 *
 * Known issues:
 * Note:
 ******************************************************************************/
sl_status_t dwin_update_open_file(const char *filename)
{
  /*
   * Ainda é necessário definir como o arquivo chegará aqui,
   * mas a ideia é passar o arquivo aberto e o seu nome para ser tratado em diante
   * e montar a struct de update que nao irá guardar o nome do arquivo e nem a extensão como string
   */
  (void)filename;

  return SL_STATUS_NOT_READY;
}

sl_status_t dwin_update_start(dwin_update_file_t *file)
{
  sl_status_t status;

  if(file == NULL ||
      file->name == NULL ||
      file->data == NULL ||
      file->size == 0U)
    return SL_STATUS_INVALID_PARAMETER;

  if(update.active)
    {
      return SL_STATUS_BUSY;
    }

  memset(&update, 0, sizeof(update));

  update.file = file;

  update.file->position = 0U;

  if(!dwin_update_extension_handler(&update))
    {
      return SL_STATUS_INVALID_PARAMETER;
    }

  if(!dwin_update_file_size_handler(&status, &update))
    {
      return status;
    }

  if(!dwin_update_identify_file_handler(&update))
    {
      return SL_STATUS_INVALID_PARAMETER;
    }

  if(!dwin_update_validate_file_id_range(&update))
    {
      return SL_STATUS_INVALID_PARAMETER;
    }

  dwin_update_init_values(&update);

  dwin_update_debug_print();

  init_update_event();


  sl_zigbee_event_set_delay_ms(&dwin_update_event, 0U);
  return SL_STATUS_OK;
}

static sl_status_t load_next_chunk(void)
{
  size_t bytes_to_process;
  size_t bytes_read;
  uint32_t file_remaining;
  uint32_t block_remaining;

  update.buffer_size = 0U;
  update.buffer_offset = 0U;
  /*
   * O bloco atual já está completo
   */
  if(update.block_offset >= update.current_block_size)
    {
      return SL_STATUS_OK;
    }

  block_remaining = update.current_block_size - update.block_offset;

  /*
   * Ainda existem dados reais no arquivo.
   */

  if(update.file_offset < update.file_size)
    {
      file_remaining = update.file_size - update.file_offset;

      /*
       * Tamanho máximo permitido no buffer
       */
      bytes_to_process = sizeof(update.buffer);

      /*
       * Não pode ultrapassar o restante do arquivo
       */
      if(bytes_to_process > file_remaining)
        {
          bytes_to_process = file_remaining;
        }

      /*
       * Não pode ultrapassar o restante do bloco físico
       */
      if(bytes_to_process > block_remaining)
        {
          bytes_to_process = block_remaining;
        }

      /*
       * Os dados da DWIN precisam ser enviados em words.
       */
      if((bytes_to_process & 1U) != 0U)
        {
          bytes_to_process--;
        }

      /*
       * Caso o tamanho calculado tenha ficado zero,
       * há uma inconsistencia no tamanho do arquivo/bloco.
       */
      if(bytes_to_process == 0U)
        {
          update.error = DWIN_UPDATE_ERROR_FILE_SIZE;
          return SL_STATUS_INVALID_PARAMETER;
        }

      bytes_read = 0U;

      /*
       * Lê os dados do arquivo virtual
       */
      if(!dwin_update_file_read(update.file, update.buffer, bytes_to_process, &bytes_read))
        {
          update.error = DWIN_UPDATE_ERROR_FILE_READ;
          return SL_STATUS_FAIL;
        }

      /*
       * A quantidade lida deve ser exatamente a solicitada.
       */
      if(bytes_read != bytes_to_process)
        {
          update.error = DWIN_UPDATE_ERROR_FILE_READ;
          return SL_STATUS_IO;
        }

      update.buffer_size = bytes_read;
      update.buffer_offset = 0U;

      return SL_STATUS_OK;
    }

  /*
   * O arquivo terminou
   *
   * O restante do bloco atual deve ser preenchido com 0x00.
   */
  bytes_to_process = sizeof(update.buffer);

  if(bytes_to_process > block_remaining)
    {
      bytes_to_process = block_remaining;
    }

  /*
   * O tamanho do bloco e do buffer devem ser pares.
   */
  if((bytes_to_process & 1U) != 0U)
    {
      bytes_to_process--;
    }

  if(bytes_to_process == 0U)
    {
      update.error = DWIN_UPDATE_ERROR_FILE_SIZE;
      return SL_STATUS_INVALID_PARAMETER;
    }

  memset(update.buffer,
         DWIN_UPDATE_FILL_VALUE,
         bytes_to_process);

  update.buffer_size = bytes_to_process;
  update.buffer_offset = 0U;

  return SL_STATUS_OK;
}

// pedir implementação nova
static sl_status_t send_buffer_to_ram(void)
{
  size_t packet_size;

  if(update.buffer_offset >= update.buffer_size)
    {
      return SL_STATUS_INVALID_STATE;
    }

  packet_size = update.buffer_size - update.buffer_offset;

  if(packet_size > DWIN_UPDATE_PACKET_SIZE)
    {
      packet_size = DWIN_UPDATE_PACKET_SIZE;
    }

  if((packet_size & 1U) != 0U)
    {
      packet_size--;
    }

  if(packet_size == 0U)
    {
      return SL_STATUS_INVALID_PARAMETER;
    }

  update.current_packet_size = packet_size;

  return dwin_write_vp_async(
      update.ram_address,
      &update.buffer[update.buffer_offset],
      packet_size,
      DWIN_UPDATE_RAM_WRITE_TIMEOUT_MS,
      dwin_update_ram_write_ack_callback,
      NULL);
}

static void dwin_update_ram_write_ack_callback(sl_status_t status, uint16_t vp, void *context)
{
  (void)vp;
  (void)context;

  size_t packet_size;

  if(status != SL_STATUS_OK)
    {
      if(dwin_update_is_recoverable_error(status) &&
          update.retry_count < DWIN_UPDATE_MAX_RETRIES)
        {
          update.retry_count++;

          printf("Falha escrita RAM - retry %u/%u\r\n", update.retry_count, DWIN_UPDATE_MAX_RETRIES);

          update.state = DWIN_UPDATE_STATE_WRITE_RAM;
          return;
        }

      update.error = DWIN_UPDATE_ERROR_DWIN_WRITE;
      update.state = DWIN_UPDATE_STATE_ERROR;

      return;
    }

  /*
   * Agora o pacote foi confirmado pela DWIN
   */

  packet_size = update.current_packet_size;

  update.ram_address += (uint16_t)(packet_size / 2U);
  update.block_offset += (uint32_t)packet_size;

  /*
   * Só contabiliza bytes reais do arquivo
   */
  if(update.file_offset < update.file_size)
    {
      uint32_t file_remaining = update.file_size - update.file_offset;

      size_t file_bytes = update.current_packet_size;

      if(file_bytes > file_remaining)
        {
          file_bytes = file_remaining;
        }

      update.file_offset += (uint32_t)file_bytes;
    }

  update.buffer_offset += update.current_packet_size;

  update.retry_count = 0U;

  if(update.buffer_offset < update.buffer_size)
    {
      update.state = DWIN_UPDATE_STATE_WRITE_RAM;
      return;
    }

  update.buffer_size = 0U;
  update.buffer_offset = 0U;

  if(update.block_offset < update.current_block_size)
    {
      update.state = DWIN_UPDATE_STATE_LOAD_BLOCK;
      return;
    }

  update.state = DWIN_UPDATE_STATE_FLASH_WRITE;
}

static sl_status_t dwin_update_write_ram_packet(uint16_t ram_address, size_t packet_size, const uint8_t *data)
{
  sl_status_t status;
  uint8_t retries = 0U;

  while(true)
    {
      //      status = dwin_write(update.ram_address,
      //                    packet_size,
      //                    &update.buffer[offset]);
      status = dwin_test_write_ram(ram_address, packet_size, data);

      if(status == SL_STATUS_OK)
        {
          return SL_STATUS_OK;
        }

      if(!dwin_update_is_recoverable_error(status))
        {
          return status;
        }

      if(retries >= DWIN_UPDATE_MAX_RETRIES)
        {
          return status;
        }

      retries++;

      printf("Falha escrita RAM - retry %u/%u - status 0x%02X\r\n", retries, DWIN_UPDATE_MAX_RETRIES, (unsigned int) status);
    }
}

static sl_status_t fill_ram_with_zero(void)
{
  static uint8_t zero_buffer[DWIN_UPDATE_BUFFER_SIZE];

  size_t remaining;
  size_t fill_size;

  memset(zero_buffer,
         DWIN_UPDATE_FILL_VALUE,
         sizeof(zero_buffer));

  remaining = dwin_update_get_block_size() - update.block_offset;

  while(remaining > 0U)
    {
      fill_size = remaining;

      if(fill_size > sizeof(zero_buffer))
        fill_size = sizeof(zero_buffer);

      if(dwin_write(update.ram_address, zero_buffer, fill_size) != SL_STATUS_OK)
        {
          return SL_STATUS_FAIL;
        }

      update.ram_address += (uint16_t)(fill_size / 2U);

      update.block_offset += (uint32_t)fill_size;

      remaining -= fill_size;
    }

  return SL_STATUS_OK;
}

static uint32_t dwin_update_get_block_size(void)
{
  switch(update.method)
  {
    case DWIN_UPDATE_METHOD_0xAA:
      return DWIN_UPDATE_FLASH_BLOCK_SIZE_0XAA;

    case DWIN_UPDATE_METHOD_0x06:
      return DWIN_UPDATE_FLASH_BLOCK_SIZE_0X06;

    default:
      return 0U;
  }
}

static sl_status_t dwin_update_flash_write_with_retry(void)
{
  sl_status_t status;
  uint8_t retries = 0U;

  while(true)
    {
      if(update.method == DWIN_UPDATE_METHOD_0xAA)
        {
          //      *status = dwin_update_flash_write_block(
          //          (uint16_t)update.current_block,
          //          DWIN_UPDATE_RAM_START,
          //          0U);
          status = dwin_test_flash_write_block((uint16_t)update.current_block, DWIN_UPDATE_RAM_START, 0U);
        }
      else
        {
          status = dwin_os_update_block((uint16_t)update.current_block, DWIN_UPDATE_RAM_START, 0U);
        }

      if(status == SL_STATUS_OK)
        {
          return SL_STATUS_OK;
        }

      if(!dwin_update_is_recoverable_error(status))
        {
          return status;
        }

      if(retries >= DWIN_UPDATE_MAX_RETRIES)
        {
          return status;
        }

      retries++;

      printf("Falha gravacao Flash - retry %u/%u - status 0x%02X\r\n", retries, DWIN_UPDATE_MAX_RETRIES, (unsigned int) status);
    }
}

static sl_status_t dwin_test_flash_write_block(uint16_t flash_block, uint16_t ram_address, uint16_t delay_ms)
{
  uint32_t ram_offset;

  (void)flash_block;
  (void)delay_ms;

  if(ram_address < DWIN_UPDATE_RAM_START)
    {
      return SL_STATUS_INVALID_PARAMETER;
    }

  ram_offset = ((uint32_t)ram_address - DWIN_UPDATE_RAM_START) * 2U;

  if(ram_offset + DWIN_UPDATE_FLASH_BLOCK_SIZE_0XAA > DWIN_TEST_RAM_SIZE)
    {
      return SL_STATUS_WOULD_OVERFLOW;
    }

  memcpy(&dwin_test_flash, &dwin_test_ram[ram_offset], DWIN_UPDATE_FLASH_BLOCK_SIZE_0XAA);

  return SL_STATUS_OK;
}

static sl_status_t dwin_os_update_block(uint16_t flash_block,
                                        uint16_t ram_address,
                                        uint16_t delay_ms)
{
  /*
   * Ainda será implementado a atualização do arquivo DWINOS, retorno somente para não gerar erro
   */
  return SL_STATUS_NOT_READY;
}

static void init_update_event(void)
{
  if(update_event_initialized)
    return;

  sl_zigbee_event_init(&dwin_update_event, update_event_handler);

  update_event_initialized = true;
}

static void update_event_handler(sl_zigbee_event_t *event)
{
  (void)event;

  dwin_update_process();

  if(update.active)
    sl_zigbee_event_set_delay_ms(&dwin_update_event, 10U);
  else
    sl_zigbee_event_set_inactive(&dwin_update_event);
}

void dwin_update_process(void)
{
  sl_status_t status = SL_STATUS_OK;

  if(!update.active)
    return;

  switch(update.state)
  {
    case DWIN_UPDATE_STATE_ENABLE_CRC:
      dwin_update_case_enable_crc(&status);
      break;

    case DWIN_UPDATE_STATE_WAIT_CRC_ENABLES:
      dwin_update_case_wait_crc_enables(&status);
      break;

    case DWIN_UPDATE_STATE_LOAD_BLOCK:
      dwin_update_case_load_block(&status);
      break;

    case DWIN_UPDATE_STATE_WRITE_RAM:
      dwin_update_case_write_ram(&status);
      break;

    case DWIN_UPDATE_STATE_FILL_BLOCK:
      dwin_update_case_fill_block(&status);
      break;

    case DWIN_UPDATE_STATE_FLASH_WRITE:
      dwin_update_case_flash_write(&status);
      break;

    case DWIN_UPDATE_STATE_WAIT_FLASH:
      dwin_update_case_wait_flash(&status);
      break;

    case DWIN_UPDATE_STATE_VERIFY_BLOCK:
      dwin_update_case_verify_block(&status);
      break;

    case DWIN_UPDATE_STATE_NEXT_BLOCK:
      dwin_update_case_next_block(&status);
      break;

    case DWIN_UPDATE_STATE_FINISH:
      dwin_update_case_update_finish(&status);
      break;

    case DWIN_UPDATE_STATE_ERROR:
      dwin_update_case_error(&status);
      break;

    default:
      break;
  }
}

static void dwin_update_case_enable_crc(sl_status_t *status)
{
  *status = dwin_enable_crc();

  if(*status != SL_STATUS_OK)
    {
      update.state = DWIN_UPDATE_STATE_ERROR;
      return;
    }

  update.state = DWIN_UPDATE_STATE_WAIT_CRC_ENABLES;
}

static void dwin_update_case_wait_crc_enables(sl_status_t *status)
{
  if(dwin_is_crc_enabled())
    {
      update.state = DWIN_UPDATE_STATE_LOAD_BLOCK;
    }
}

static void dwin_update_case_load_block(sl_status_t *status)
{
  if(update.block_offset >= update.current_block_size)
    {
      update.state = DWIN_UPDATE_STATE_FLASH_WRITE;
      return;
    }

  *status = load_next_chunk();

  if(*status != SL_STATUS_OK)
    {
      update.state = DWIN_UPDATE_STATE_ERROR;
      return;
    }

  update.state = DWIN_UPDATE_STATE_WRITE_RAM;
}

static void dwin_update_case_fill_block(sl_status_t *status)
{
  /*
   * Não está mais sendo utilizado, pode ser que seja utilizado futuramente para o 0x06
   */
  *status = fill_ram_with_zero();

  if(*status != SL_STATUS_OK)
    {
      update.error = DWIN_UPDATE_ERROR_DWIN_WRITE;
      update.state = DWIN_UPDATE_STATE_ERROR;
      return;
    }

  if(update.method == DWIN_UPDATE_METHOD_0xAA)
    {
      if(update.block_offset != DWIN_UPDATE_FLASH_BLOCK_SIZE_0XAA)
        {
          update.error = DWIN_UPDATE_ERROR_DWIN_WRITE;
          update.state = DWIN_UPDATE_STATE_ERROR;
          return;
        }
    }
  else
    {
      if(update.block_offset >= DWIN_UPDATE_FLASH_BLOCK_SIZE_0X06)
        {
          update.error = DWIN_UPDATE_ERROR_DWIN_WRITE;
          update.state = DWIN_UPDATE_STATE_ERROR;
          return;
        }
    }

  update.state = DWIN_UPDATE_STATE_FLASH_WRITE;
}

static void dwin_update_case_write_ram(sl_status_t *status)
{
  *status = send_buffer_to_ram();

  if(*status != SL_STATUS_OK)
    {
      if(dwin_update_is_recoverable_error(*status) &&
          update.retry_count < DWIN_UPDATE_MAX_RETRIES)
        {
          update.retry_count++;

          printf("Falha envio RAM - retry %u/%u - status 0x%02X\r\n", update.retry_count, DWIN_UPDATE_MAX_RETRIES, (unsigned int)*status);
          return;
        }
      update.error = DWIN_UPDATE_ERROR_DWIN_WRITE;
      update.state = DWIN_UPDATE_STATE_ERROR;
      return;
    }

  update.state = DWIN_UPDATE_STATE_FLASH_WRITE;
}

static void dwin_update_case_flash_write(sl_status_t *status)
{
  /*
   * O comando 0xAA exige um bloco físico completo de 32Kb
   *
   * O preenchimento com 0x00 do último trecho do arquivo já foi
   * realizado por load_next_chunk(), portanto nunca chega aqui
   * com um bloco parcial.
   */

  if(update.method == DWIN_UPDATE_METHOD_0xAA)
    {
      if(update.block_offset != DWIN_UPDATE_FLASH_BLOCK_SIZE_0XAA)
        {
          update.error = DWIN_UPDATE_ERROR_FILE_SIZE;
          update.state = DWIN_UPDATE_STATE_ERROR;
          return;
        }
    }

  else
    {
      if(update.block_offset != DWIN_UPDATE_FLASH_BLOCK_SIZE_0X06)
        {
          update.error = DWIN_UPDATE_ERROR_FILE_SIZE;

          update.state = DWIN_UPDATE_STATE_ERROR;

          return;
        }
    }

  *status = dwin_update_flash_write_with_retry();

  if(*status != SL_STATUS_OK)
    {
      update.error = DWIN_UPDATE_ERROR_DWIN_WRITE;
      update.state = DWIN_UPDATE_STATE_ERROR;
      return;
    }

  update.flash_status_pending = false;
  update.state = DWIN_UPDATE_STATE_WAIT_FLASH;
}

static void dwin_update_case_wait_flash(sl_status_t *status)
{
  if(update.flash_status_pending)
    {
      return;
    }

  *status = dwin_update_request_flash_status_with_retry();

  if(*status != SL_STATUS_OK)
    {
      update.error = DWIN_UPDATE_ERROR_DWIN_STATUS;
      update.state = DWIN_UPDATE_STATE_ERROR;
      return;
    }

  /*
   * O callback decide:
   *
   * 5A02 -> WAIT_FLASH
   * 0002 -> NEXT_BLOCK
   */

  update.flash_status_pending = true;
}

static void dwin_update_case_verify_block(sl_status_t *status)
{
  uint32_t block_index;

  (void)status;

  block_index = update.current_block - update.file_id_base_block;

  if(!dwin_test_verify_flash_block(block_index))
    {
      if(update.retry_count < DWIN_UPDATE_MAX_RETRIES)
        {
          update.retry_count++;

          printf("Falha verificacao bloco %lu - retry %u/%u\r\n", (unsigned long)block_index, update.retry_count, DWIN_UPDATE_MAX_RETRIES);

          dwin_update_restart_current_block();

          update.state = DWIN_UPDATE_STATE_LOAD_BLOCK;

          return;
        }

      printf("Bloco %lu falhou apos %u retries\r\n", (unsigned long)block_index, DWIN_UPDATE_MAX_RETRIES);

      update.error = DWIN_UPDATE_ERROR_FLASH_WRITE;
      update.state = DWIN_UPDATE_STATE_ERROR;

      return;
    }

  update.progress = dwin_update_get_progress();

  printf("Current block: %lu / %lu\r\n",
         (unsigned long)update.current_block,
         (unsigned long)(update.file_id_base_block + update.total_blocks - 1U));

  printf("Bloco %lu verificado com sucesso! (%u%%)\r\n", (unsigned long) block_index, update.progress);


  update.state = DWIN_UPDATE_STATE_NEXT_BLOCK;
}

static void dwin_update_case_next_block(sl_status_t *status)
{
  uint32_t blocks_completed;

  update.block_offset = 0U;
  update.ram_address = DWIN_UPDATE_RAM_START;
  update.buffer_size = 0U;
  update.retry_count = 0U;

  /*
   * current_block é baseado em um endereço absoluto
   * do Flash.
   */
  blocks_completed =
      update.current_block -
      update.file_id_base_block;

  /*
   * O último bloco necessário para todos os IDs já foi
   * gravado.
   */
  if((blocks_completed + 1U) >= update.total_blocks)
  {
      update.state = DWIN_UPDATE_STATE_FINISH;
      return;
  }

  update.current_block++;

  update.current_block_size =
      dwin_update_get_block_size();

  update.block_file_offset = update.file_offset;

  update.state = DWIN_UPDATE_STATE_LOAD_BLOCK;
}

static void dwin_update_case_update_finish(sl_status_t *status)
{
  if(update.file != NULL)
    {
      dwin_update_file_close(update.file);
    }

  printf("Atualizacao concluida com sucesso!\r\n");
  update.active = false;

  return;
}

static void dwin_update_case_error(sl_status_t *status)
{
  /*
   * Sugestão para um "tentar novamente":
   * Troca para uma página que apresentará esta mensagem com um botão para aceitar ou recusar
   * Funcionamento deste método ficaria algo como:
   * Troca para página X
   * Cria um callback para cada botão (não é possível criar dois callbacks iguais então isso não geraria erro)
   * Se quiser tentar novamente, irá recomeçar a atualização, senão irá cancelar a atualização e executar este comando de close
   */
//  dwin_change_page(DWIN_PAGE_RETRY_UPDATE);
//
//  dwin_register_callback(DWIN_VP_BUTTON_RETRY,
//                         DWIN_CMD_READ,
//                         true,
//                         0,
//                         dwin_update_confirm_retry_callback);
//
//  dwin_register_callback(DWIN_VP_BUTTON_RETRY,
//                         DWIN_CMD_READ,
//                         true,
//                         1,
//                         dwin_update_cancel_retry_callback);

  if(update.file != NULL)
    {
      dwin_update_file_close(update.file);
    }

  update.active = false;
  return;
}

//static void dwin_update_confirm_retry_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context)
//{
//  dwin_update_start(update.file);
//}
//
//static void dwin_update_cancel_retry_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context)
//{
//  if(update.file != NULL)
//    {
//      dwin_update_file_close(update.file);
//    }
//
//  update.active = false;
//}

sl_status_t dwin_update_request_flash_status_with_retry(void)
{
  sl_status_t status;
  uint8_t retries = 0U;

  while(true)
    {
      status = dwin_update_flash_request_status();

      if(status == SL_STATUS_OK)
        {
          return SL_STATUS_OK;
        }

      if(!dwin_update_is_recoverable_error(status))
        {
          return status;
        }

      if(retries >= DWIN_UPDATE_MAX_RETRIES)
        {
          return status;
        }

      retries++;

      printf("Falha consulta status Flash - retry %u/%u - status 0x%02X\r\n", retries, DWIN_UPDATE_MAX_RETRIES, (unsigned int) status);
    }
}

static bool dwin_update_is_recoverable_error(sl_status_t status)
{
  switch(status)
  {
    case SL_STATUS_BUSY:
    case SL_STATUS_TIMEOUT:
    case SL_STATUS_IO:
    case SL_STATUS_FAIL:
      return true;

    default:
      return false;
  }
}

bool dwin_update_is_active(void)
{
  return update.active;
}

uint8_t dwin_update_get_progress(void)
{
  uint32_t completed_bytes;
  uint32_t total_bytes;

  if(update.total_blocks == 0U)
    {
      return 0U;
    }

  total_bytes = (uint64_t)update.total_blocks * DWIN_UPDATE_FLASH_BLOCK_SIZE_0XAA;

  completed_bytes = (uint64_t)(update.current_block - update.file_id_base_block) * DWIN_UPDATE_FLASH_BLOCK_SIZE_0XAA;

  completed_bytes += update.block_offset;

  if(completed_bytes >= total_bytes)
    {
      return 100U;
    }

  return (uint8_t)((completed_bytes * 100U) / total_bytes);
}

dwin_update_state_t dwin_update_get_state(void)
{
  return update.state;
}

dwin_update_error_t dwin_update_get_error(void)
{
  return update.error;
}

static sl_status_t dwin_test_write_ram(uint16_t ram_address, size_t size, const uint8_t *data)
{
  uint32_t byte_offset;

  if(data == NULL)
    {
      return SL_STATUS_NULL_POINTER;
    }

  if((size & 1U) != 0U)
    {
      return SL_STATUS_INVALID_PARAMETER;
    }

  if(ram_address < DWIN_UPDATE_RAM_START)
    {
      return SL_STATUS_INVALID_PARAMETER;
    }

  byte_offset = ((uint32_t)ram_address - DWIN_UPDATE_RAM_START) * 2U;

  if(byte_offset + size > DWIN_TEST_RAM_SIZE)
    {
      return SL_STATUS_WOULD_OVERFLOW;
    }

  memcpy(&dwin_test_ram[byte_offset], data, size);

  return SL_STATUS_OK;
}

static bool dwin_test_verify_flash_block(uint32_t block_index)
{
  uint32_t expected_offset;
  size_t i;
  uint8_t expected_value;

  expected_offset = block_index * DWIN_UPDATE_FLASH_BLOCK_SIZE_0XAA;

  for(i = 0U; i < DWIN_UPDATE_FLASH_BLOCK_SIZE_0XAA; i++)
    {
      /*
       * Ainda existe conteúdo real do arquivo.
       */
      if((expected_offset + i) < update.file_size)
        {
          expected_value = update.file->data[expected_offset + i];
        }
      else
        {
          /*
           * O arquivo terminou.
           * O restante deve ser 0x00.
           */
          expected_value = DWIN_UPDATE_FILL_VALUE;
        }
      if(dwin_test_flash[i] != expected_value)
        {
          printf("ERRO BLOCO %lu - offset %lu - esperado 0x%02X - recebido 0x%02X\r\n", (unsigned long)block_index, (unsigned long)(expected_offset + i), expected_value, dwin_test_flash[i]);
          return false;
        }
    }

  return true;
}

static void dwin_update_restart_current_block(void)
{
  update.file_offset = update.block_file_offset;

  update.file->position = (size_t)update.block_file_offset;

  update.block_offset = 0U;
  update.ram_address = DWIN_UPDATE_RAM_START;
  update.buffer_size = 0U;
}

static void dwin_update_debug_print(void)
{
  const char *extension_name;
  const char *method_name;

  switch(update.extension)
  {
    case DWIN_UPDATE_EXTENSION_BIN:
      extension_name = "BIN";
      break;
    case DWIN_UPDATE_EXTENSION_HZK:
      extension_name = "HZK";
      break;
    case DWIN_UPDATE_EXTENSION_ICL:
      extension_name = "ICL";
      break;
    case DWIN_UPDATE_EXTENSION_WAE:
      extension_name = "WAE";
      break;
    default:
      extension_name = "ERROR";
      break;
  }

  switch(update.method)
  {
    case DWIN_UPDATE_METHOD_0xAA:
      method_name = "0xAA";
      break;
    case DWIN_UPDATE_METHOD_0x06:
      method_name = "0x06";
      break;
    default:
      method_name = "INVALID";
      break;
  }

  printf("\r\n");
  printf("================================================\r\n");
  printf("DWIN UPDATE\r\n");
  printf("================================================\r\n");

  printf("ID: %u\r\n", (unsigned)update.id);

  printf("Extension: %s (%u)\r\n", extension_name, (unsigned)update.extension);

  printf("Method: %s (%u)\r\n", method_name, (unsigned)update.method);

  printf("File size: %lu bytes\r\n", (unsigned long)update.file_size);

  printf("Total blocks: %lu\r\n", (unsigned long)update.total_blocks);

  printf("File ID base block: %lu (0x%04lX)\r\n",
         (unsigned long)update.file_id_base_block,
         (unsigned long)update.file_id_base_block);

  printf("================================================\r\n");
}
