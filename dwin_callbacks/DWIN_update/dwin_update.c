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
static bool *finished = NULL;
/*******************************************************************************
 * Extern
 ******************************************************************************/

/*******************************************************************************
 * Private Function Prototypes
 ******************************************************************************/
static sl_status_t load_next_chunk(void);
static sl_status_t send_buffer_to_ram(void);
static void dwin_update_ram_write_ack_callback(sl_status_t status, uint16_t vp, void *context);
static uint32_t dwin_update_get_block_size(void);
//static sl_status_t dwin_os_update_block(uint16_t flash_block,
//                                        uint16_t ram_address,
//                                        uint16_t delay_ms);
static void init_update_event(void);
static void update_event_handler(sl_zigbee_event_t *event);
static void dwin_update_case_enable_crc(sl_status_t *status);
static void dwin_update_case_wait_crc_enables(sl_status_t *status);
static void dwin_update_case_load_block(sl_status_t *status);
static void dwin_update_case_write_ram(sl_status_t *status);
static void dwin_update_case_wait_ram_ack(sl_status_t *status);
static void dwin_update_case_flash_write(sl_status_t *status);
static void dwin_update_case_wait_flash(sl_status_t *status);
static void dwin_update_case_next_block(sl_status_t *status);
static void dwin_update_case_disable_crc(sl_status_t *status);
static void dwin_update_case_wait_crc_disable(sl_status_t *status);
static void dwin_update_case_error_wait_crc_disable(sl_status_t *status);
static void dwin_update_case_update_finish(sl_status_t *status);
static void dwin_update_case_error(sl_status_t *status);
//static void dwin_update_confirm_retry_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context);
//static void dwin_update_cancel_retry_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context);
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

sl_status_t dwin_update_start(dwin_update_file_t *file, bool *finish)
{
  sl_status_t status;

  if(file == NULL ||
      file->name == NULL ||
      file->data == NULL ||
      file->size == 0U ||
      finish == NULL)
    return SL_STATUS_INVALID_PARAMETER;

  if(update.active)
    {
      return SL_STATUS_BUSY;
    }

  memset(&update, 0, sizeof(update));

  finished = finish;
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

  sl_zigbee_event_set_delay_ms(&dwin_update_event, 10U);

  return SL_STATUS_OK;
}

static sl_status_t load_next_chunk(void)
{
  size_t data_size;
  size_t buffer_size;
  size_t bytes_read;
  size_t file_remaining;
  size_t block_remaining;

  update.buffer_size = 0U;
  update.buffer_offset = 0U;

  if(update.file_offset > update.file_size)
    {
      update.error = DWIN_UPDATE_ERROR_FILE_SIZE;
      return SL_STATUS_INVALID_STATE;
    }
  /*
   * O bloco atual já está completo
   */
  if(update.block_offset >= update.current_block_size)
    {
      return SL_STATUS_OK;
    }

  file_remaining = (size_t)(update.file_size - update.file_offset);

  block_remaining = (size_t)(update.current_block_size - update.block_offset);

  /*
   * Ainda existe conteúdo real do arquivo
   */
  if(file_remaining > 0U)
    {
      data_size = sizeof(update.buffer);

      if(data_size > file_remaining)
        {
          data_size = file_remaining;
        }

      if(data_size > block_remaining)
        {
          data_size = block_remaining;
        }

      if(data_size == 0U)
        {
          update.error = DWIN_UPDATE_ERROR_FILE_SIZE;
          return SL_STATUS_INVALID_PARAMETER;
        }

      /*
       * O protocolo trabalha com words.
       * Se o ultimo trecho real for impar,
       * acrescenta um byte 0x00
       */

      buffer_size = data_size;

      if((data_size & 1U) != 0U)
        {
          if(data_size >= sizeof(update.buffer) ||
              data_size >= block_remaining)
            {
              update.error = DWIN_UPDATE_ERROR_FILE_SIZE;
              return SL_STATUS_INVALID_PARAMETER;
            }

          buffer_size++;

          update.buffer[data_size] = DWIN_UPDATE_FILL_VALUE;
        }

      /*
       * file_offset é a posição lógica do arquivo.
       * O position da estrutura é sincronizado antes da leitura
       */

      update.file->position = (size_t)update.file_offset;

      bytes_read = 0U;

      if(!dwin_update_file_read(update.file,
                                update.buffer,
                                data_size,
                                &bytes_read))
        {
          update.error = DWIN_UPDATE_ERROR_FILE_READ;
          return SL_STATUS_FAIL;
        }

      /*
       * Aqui compara com os bytes realmente lidos
       * do arquivo, não com o tamanho após padding
       */
      if(bytes_read != data_size)
        {
          update.error = DWIN_UPDATE_ERROR_FILE_READ;
          return SL_STATUS_IO;
        }

      update.buffer_size = buffer_size;
      update.buffer_offset = 0U;

      return SL_STATUS_OK;
    }

  /*
   * O arquivo terminou.
   * Completa o restante do bloco físico com 0x00
   */
  buffer_size = sizeof(update.buffer);

  if(buffer_size > block_remaining)
    {
      buffer_size = block_remaining;
    }

  /*
   * O bloco 0xAA possui tamanho par e o offset
   * também deve permanecer alinhado em words
   */
  if((buffer_size & 1U) != 0U)
    {
      buffer_size--;
    }

  if(buffer_size == 0U)
    {
      update.error = DWIN_UPDATE_ERROR_FILE_SIZE;
      return SL_STATUS_INVALID_PARAMETER;
    }

  memset(update.buffer,
         DWIN_UPDATE_FILL_VALUE,
         buffer_size);

  update.buffer_size = buffer_size;
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

  size_t file_remaining;
  size_t file_bytes_acked;

  if(status != SL_STATUS_OK)
    {
      if(dwin_update_is_recoverable_error(status) &&
          update.ram_retry_count < DWIN_UPDATE_MAX_RETRIES)
        {
          update.ram_retry_count++;

          printf("Falha escrita RAM - retry %u/%u\r\n", update.ram_retry_count, DWIN_UPDATE_MAX_RETRIES);

          update.state = DWIN_UPDATE_STATE_WRITE_RAM;
          return;
        }

      update.error = DWIN_UPDATE_ERROR_DWIN_WRITE;
      update.state = DWIN_UPDATE_STATE_ERROR;
      return;
    }

  file_remaining = 0U;

  if(update.file_offset < update.file_size)
    {
      file_remaining = (size_t)(update.file_size - update.file_offset);
    }

  file_bytes_acked = update.current_packet_size;

  if(file_bytes_acked > file_remaining)
    {
      file_bytes_acked = file_remaining;
    }

  update.file_offset += (uint32_t)file_bytes_acked;
  update.buffer_offset += update.current_packet_size;
  update.block_offset += (uint32_t)update.current_packet_size;

  /*
   * Cada palavra ocupa 2 bytes no VP
   */
  update.ram_address += (uint16_t)(update.current_packet_size / 2U);
  update.ram_retry_count = 0U;

  /*
   * Ainda há dados no buffer
   */
  if(update.buffer_offset < update.buffer_size)
    {
      update.state = DWIN_UPDATE_STATE_WRITE_RAM;
      return;
    }

  /*
   * O buffer atual terminou, mas o bloco de 32Kib ainda não
   */
  if(update.block_offset < update.current_block_size)
    {
      update.state = DWIN_UPDATE_STATE_LOAD_BLOCK;
      return;
    }

  update.state = DWIN_UPDATE_STATE_FLASH_WRITE;
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

//static sl_status_t dwin_os_update_block(uint16_t flash_block,
//                                        uint16_t ram_address,
//                                        uint16_t delay_ms)
//{
//  /*
//   * Ainda será implementado a atualização do arquivo DWINOS, retorno somente para não gerar erro
//   */
//  return SL_STATUS_NOT_READY;
//}

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
    {
      sl_zigbee_event_set_delay_ms(&dwin_update_event, 10U);
    }
  else
    {
      printf("Update Inativo\r\n");
      sl_zigbee_event_set_inactive(&dwin_update_event);
    }
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

    case DWIN_UPDATE_STATE_WAIT_CRC_ENABLE:
      dwin_update_case_wait_crc_enables(&status);
      break;

    case DWIN_UPDATE_STATE_LOAD_BLOCK:
      dwin_update_case_load_block(&status);
      break;

    case DWIN_UPDATE_STATE_WRITE_RAM:
      dwin_update_case_write_ram(&status);
      break;

    case DWIN_UPDATE_STATE_WAIT_RAM_ACK:
      dwin_update_case_wait_ram_ack(&status);
      break;

    case DWIN_UPDATE_STATE_FLASH_WRITE:
      dwin_update_case_flash_write(&status);
      break;

    case DWIN_UPDATE_STATE_WAIT_FLASH:
      dwin_update_case_wait_flash(&status);
      break;

    case DWIN_UPDATE_STATE_NEXT_BLOCK:
      dwin_update_case_next_block(&status);
      break;

    case DWIN_UPDATE_STATE_DISABLE_CRC:
      dwin_update_case_disable_crc(&status);
      break;

    case DWIN_UPDATE_STATE_WAIT_CRC_DISABLE:
      dwin_update_case_wait_crc_disable(&status);
      break;

    case DWIN_UPDATE_STATE_ERROR_WAIT_CRC_DISABLE:
      dwin_update_case_error_wait_crc_disable(&status);
      break;

    case DWIN_UPDATE_STATE_FINISH:
      dwin_update_case_update_finish(&status);
      break;

    case DWIN_UPDATE_STATE_ERROR:
      dwin_update_case_error(&status);
      break;

    default:
      update.error = DWIN_UPDATE_ERROR_INVALID_STATE;
      update.state = DWIN_UPDATE_STATE_ERROR;
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

  update.state = DWIN_UPDATE_STATE_WAIT_CRC_ENABLE;
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

static void dwin_update_case_write_ram(sl_status_t *status)
{
  *status = send_buffer_to_ram();

  if(*status != SL_STATUS_OK)
    {
      update.error = DWIN_UPDATE_ERROR_DWIN_WRITE;
      update.state = DWIN_UPDATE_STATE_ERROR;
      return;
    }

  update.state = DWIN_UPDATE_STATE_WAIT_RAM_ACK;
}

static void dwin_update_case_wait_ram_ack(sl_status_t *status)
{
  (void) status;
}

static void dwin_update_case_flash_write(sl_status_t *status)
{
  if(update.block_offset != update.current_block_size)
    {
      update.error = DWIN_UPDATE_ERROR_FILE_SIZE;
      update.state = DWIN_UPDATE_STATE_ERROR;
      return;
    }

  *status = dwin_update_flash_write_block();

  if(*status != SL_STATUS_OK)
    {
      if(dwin_update_is_recoverable_error(*status) &&
          update.flash_retry_count < DWIN_UPDATE_MAX_RETRIES)
        {
          update.flash_retry_count++;
          return;
        }
      update.error = DWIN_UPDATE_ERROR_DWIN_WRITE;
      update.state = DWIN_UPDATE_STATE_ERROR;
      return;
    }

  update.state = DWIN_UPDATE_STATE_WAIT_FLASH;
}

static void dwin_update_case_wait_flash(sl_status_t *status)
{
  if(update.flash_status_pending)
    {
      return;
    }

  update.flash_status_pending = true;

  *status = dwin_update_flash_request_status();

  if(*status != SL_STATUS_OK)
    {
      update.flash_status_pending = false;

      if(dwin_update_is_recoverable_error(*status) &&
          update.flash_status_retry_count < DWIN_UPDATE_MAX_RETRIES)
        {
          update.flash_status_retry_count++;
          return;
        }

      update.error = DWIN_UPDATE_ERROR_DWIN_STATUS;
      update.state = DWIN_UPDATE_STATE_ERROR;
    }
}

static void dwin_update_case_next_block(sl_status_t *status)
{
  uint32_t blocks_completed;

  (void) status;

  /*
   * current_block é baseado em um endereço absoluto
   * do Flash.
   */
  update.progress = dwin_update_get_progress();

  blocks_completed =
      update.current_block -
      update.file_id_base_block;

  printf("Bloco %ld completo, atualizacao %u%% completa\r\n", blocks_completed, update.progress);

  /*
   * O último bloco necessário para todos os IDs já foi
   * gravado.
   */
  if((blocks_completed + 1U) >= update.total_blocks)
  {
      update.state = DWIN_UPDATE_STATE_DISABLE_CRC;
      return;
  }

  /*
   * Prepara o próximo bloco
   */
  update.block_offset = 0U;
  update.ram_address = DWIN_UPDATE_RAM_START;
  update.buffer_size = 0U;
  update.buffer_offset = 0U;

  update.current_block++;

  update.current_block_size = dwin_update_get_block_size();

  update.block_file_offset = update.file_offset;

  update.ram_retry_count = 0U;
  update.flash_retry_count = 0U;
  update.flash_status_retry_count = 0U;

  update.state = DWIN_UPDATE_STATE_LOAD_BLOCK;
}

static void dwin_update_case_disable_crc(sl_status_t *status)
{
  *status = dwin_disable_crc();

  if(*status != SL_STATUS_OK)
    {
      update.state = DWIN_UPDATE_STATE_ERROR;
      return;
    }

  update.state = DWIN_UPDATE_STATE_WAIT_CRC_DISABLE;
}

static void dwin_update_case_wait_crc_disable(sl_status_t *status)
{
  (void) status;

  if(dwin_is_crc_enabled())
    {
      return;
    }

  update.state = DWIN_UPDATE_STATE_FINISH;
}

static void dwin_update_case_error_wait_crc_disable(sl_status_t *status)
{
  (void) status;

  if(dwin_is_crc_enabled())
    {
      return;
    }

  if(update.file != NULL)
    {
      dwin_update_file_close(update.file);
    }

  printf("ERRO");
  update.active = false;
}

static void dwin_update_case_update_finish(sl_status_t *status)
{
  (void)status;

  if(update.file != NULL)
    {
      dwin_update_file_close(update.file);
    }

  printf("Atualizacao concluida com sucesso!\r\n");
  update.active = false;
  if(finished != NULL)
    {
      *finished = true;
    }
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

  (void)status;

  if(dwin_is_crc_enabled())
    {
      *status = dwin_disable_crc();

      if(*status != SL_STATUS_OK)
        {
          return;
        }

      update.state = DWIN_UPDATE_STATE_ERROR_WAIT_CRC_DISABLE;
      return;
    }

  if(update.file != NULL)
    {
      dwin_update_file_close(update.file);
    }

  update.active = false;
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

bool dwin_update_is_recoverable_error(sl_status_t status)
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
  uint64_t completed_bytes;
  uint64_t total_bytes;
  uint32_t block_size;

  block_size = dwin_update_get_block_size();

  if(update.total_blocks == 0U ||
      block_size == 0U)
    {
      return 0U;
    }

  total_bytes = (uint64_t)update.total_blocks * block_size;

  completed_bytes = (uint64_t)(update.current_block - update.file_id_base_block) * block_size;

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
