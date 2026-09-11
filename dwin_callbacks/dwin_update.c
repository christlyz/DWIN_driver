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
#include "dwin_service.h"
#include "zigbee_app_framework_event.h"
#include <string.h>
/*******************************************************************************
 * Data types
 ******************************************************************************/
static dwin_update_t update;
static sl_zigbee_event_t dwin_update_event;
static bool update_event_initialized = false;
/*******************************************************************************
 * Extern
 ******************************************************************************/

/*******************************************************************************
 * Private Function Prototypes
 ******************************************************************************/
static sl_status_t dwin_update_start(const char *filename, FILE *file_ptr);
static bool dwin_update_extension_handler(const char *filename);
static bool dwin_update_identify_file_handler(const char *filename);
static bool dwin_update_file_size_handler(sl_status_t *status);
static void dwin_update_init_values();
static void dwin_update_close_file(void);
static bool get_extension(const char *filename, dwin_update_extension_t *file_extension);
static bool get_file_size(FILE *file, uint32_t *size);
static bool get_file_id(const char *filename, uint8_t *file_id);
static uint32_t file_id_to_flash_block(uint8_t file_id);
static sl_status_t load_next_chunk(void);
static sl_status_t send_buffer_to_ram(void);
static sl_status_t fill_ram_with_zero(void);
static uint32_t dwin_update_get_block_size(void);
static uint32_t dwin_update_calculate_total_blocks(void);
static uint32_t dwin_update_calculate_total_ids(void);
static sl_status_t dwin_nor_write_block(uint16_t flash_block,
                                        uint16_t ram_address,
                                        uint16_t delay_ms);
static sl_status_t dwin_os_update_block(uint16_t flash_block,
                                        uint16_t ram_address,
                                        uint16_t delay_ms);
static void flash_status_callback(sl_status_t status,
                                  uint16_t vp,
                                  const uint8_t *data,
                                  size_t data_size,
                                  void *context);
static void init_update_event(void);
static sl_status_t request_flash_status(void);
static void update_event_handler(sl_zigbee_event_t *event);
static void dwin_update_case_load_block(sl_status_t *status);
static void dwin_update_case_fill_block(sl_status_t *status);
static void dwin_update_case_write_ram(sl_status_t *status);
static void dwin_update_case_flash_write(sl_status_t *status);
static void dwin_update_case_wait_flash(sl_status_t *status);
static void dwin_update_case_next_block(sl_status_t *status);
static void dwin_update_case_update_finish(sl_status_t *status);
static void dwin_update_case_error(sl_status_t *status);
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
  FILE *file_ptr;

  if(filename == NULL)
    return SL_STATUS_INVALID_PARAMETER;

  file_ptr = fopen(filename, "rb");

  return dwin_update_start(filename, file_ptr);
}

static sl_status_t dwin_update_start(const char *filename, FILE *file_ptr)
{
  sl_status_t status;

  if(filename == NULL)
    return SL_STATUS_INVALID_PARAMETER;

  if(update.active)
    return SL_STATUS_BUSY;

  memset(&update, 0, sizeof(update));

  update.file = file_ptr;

  if(update.file == NULL)
    {
      update.error = DWIN_UPDATE_ERROR_FILE_OPEN;
      return SL_STATUS_FAIL;
    }

  if(!dwin_update_extension_handler(filename))
    {
      return SL_STATUS_INVALID_PARAMETER;
    }

  if(!dwin_update_file_size_handler(&status))
    {
      return status;
    }

  if(!dwin_update_identify_file_handler(filename))
    {
      return SL_STATUS_INVALID_PARAMETER;
    }

  dwin_update_init_values();

  init_update_event();

  sl_zigbee_event_set_delay_ms(&dwin_update_event, 0U);

  return SL_STATUS_OK;
}

static bool dwin_update_extension_handler(const char *filename)
{
  if(!get_extension(filename, &update.extension))
    {
      dwin_update_close_file();

      update.error = DWIN_UPDATE_ERROR_UNSUPPORTED_FILE;
      return false;
    }
  return true;
}

static bool dwin_update_file_size_handler(sl_status_t *status)
{
  uint32_t file_size = 0U;

  *status = get_file_size(update.file, &file_size);

  if(*status != SL_STATUS_OK)
    {
      dwin_update_close_file();
      update.error = DWIN_UPDATE_ERROR_FILE_SIZE;
      return false;
    }

  if(file_size == 0U)
    {
      dwin_update_close_file();
      update.error = DWIN_UPDATE_ERROR_FILE_EMPTY;
      *status = SL_STATUS_INVALID_PARAMETER;
      return false;
    }

  if((file_size & 1U) != 0U)
    {
      dwin_update_close_file();
      update.error = DWIN_UPDATE_ERROR_FILE_SIZE;
      *status = SL_STATUS_INVALID_PARAMETER;
      return false;
    }

  update.file_size = file_size;

  *status = SL_STATUS_OK;
  return true;
}

static bool dwin_update_identify_file_handler(const char *filename)
{
  if(get_file_id(filename, &update.id))
    {
      update.method = DWIN_UPDATE_METHOD_0xAA;
      return true;
    }

  if(update.extension == DWIN_UPDATE_EXTENSION_BIN &&
      strncmp(filename, "DWINOS", 6U) == 0)
    {
      update.method = DWIN_UPDATE_METHOD_0x06;
      return true;
    }

  dwin_update_close_file();
  update.method = DWIN_UPDATE_METHOD_INVALID;
  update.error = DWIN_UPDATE_ERROR_INVALID_FILE;
  return false;
}

static void dwin_update_init_values()
{
  update.file_offset = 0U;
  update.block_offset = 0U;
  update.buffer_size = 0U;
  update.ram_address = DWIN_UPDATE_RAM_START;
  update.retry_count = 0U;
  update.flash_status_pending = false;

  if(update.method == DWIN_UPDATE_METHOD_0xAA)
    {
      update.file_id_base_block = file_id_to_flash_block(update.id);
      update.current_block = file_id_to_flash_block(update.id);

      /*
       * Quantidade total de blocos físicos necessários, incluindo o preenchimento do último ID.
       */
      update.total_blocks = dwin_update_calculate_total_blocks();
      update.current_block_size = DWIN_UPDATE_FLASH_BLOCK_SIZE_0XAA;
    }
  else
    {
      update.file_id_base_block = 0U;
      update.current_block = 0U;
      update.total_blocks = 0U;
      update.current_block_size = 0U;
    }

  update.active = true;
  update.state = DWIN_UPDATE_STATE_LOAD_BLOCK;
  update.error = DWIN_UPDATE_ERROR_NONE;
}

static void dwin_update_close_file(void)
{
  if(update.file != NULL)
    {
      fclose(update.file);
      update.file = NULL;
    }
}

static bool get_extension(const char *filename, dwin_update_extension_t *file_extension)
{
  const char *extension;

  if(filename == NULL)
    {
      return false;
    }

  extension = strrchr(filename, '.');

  if(extension == NULL)
    {
      return false;
    }

  extension++;

  if(strcmp(extension, "ICL") == 0 || strcmp(extension, "icl") == 0)
    {
      *file_extension = DWIN_UPDATE_EXTENSION_ICL;
      return true;
    }
  else if(strcmp(extension, "BIN") == 0 || strcmp(extension, "bin") == 0)
    {
      *file_extension = DWIN_UPDATE_EXTENSION_BIN;
      return true;
    }
  else if(strcmp(extension, "HZK") == 0 || strcmp(extension, "hzk") == 0)
    {
      *file_extension = DWIN_UPDATE_EXTENSION_HZK;
      return true;
    }
  else if(strcmp(extension, "WAE") == 0 || strcmp(extension, "wae") == 0)
    {
      *file_extension = DWIN_UPDATE_EXTENSION_WAE;
      return true;
    }
  else
    {
      *file_extension = DWIN_UPDATE_EXTENSION_ERROR;
      return false;
    }

  return true;
}

static bool get_file_size(FILE *file, uint32_t *size)
{
  long current_position;
  long file_size;

  if(file == NULL || size == NULL)
    return false;

  current_position = ftell(file);

  if(current_position < 0)
    return false;

  if(fseek(file, 0L, SEEK_END) != 0)
    return false;

  file_size = ftell(file);

  if(file_size < 0)
    return false;

  if(fseek(file, current_position, SEEK_SET) != 0)
    return false;

  if((unsigned long)file_size > UINT32_MAX)
    return false;

  *size = (uint32_t)file_size;

  return true;
}

static bool get_file_id(const char *filename, uint8_t *file_id)
{
  uint32_t value = 0U;
  size_t i = 0U;

  if(filename == NULL || file_id == NULL)
    return false;

  /*
   * O primeiro caractere precisa ser um número
   */
  if(!isdigit((unsigned char)filename[0]))
    return false;

  /*
   * Lê todos os dígitos no início do nome
   */
  while (isdigit((unsigned char)filename[i]))
  {
      value = (value * 10U) +
              (uint32_t)(filename[i] - '0');

      if (value > UINT8_MAX)
          return false;

      i++;
  }

  /*
   * É válido parar em:
   * '.'  -> extensão
   * '_'  -> nome opcional
   * letra -> nome opcional
   *
   * Portanto não precisa validar o restante.
   */

  *file_id = (uint8_t)value;

  return true;
}

static uint32_t file_id_to_flash_block(uint8_t file_id)
{
  return (uint32_t)file_id * 8U;
}

static sl_status_t load_next_chunk(void)
{
  size_t bytes_to_process;
  size_t bytes_read;
  uint32_t file_remaining;
  uint32_t block_remaining;

  update.buffer_size = 0U;

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

      bytes_to_process = sizeof(update.buffer);

      if(bytes_to_process > file_remaining)
        {
          bytes_to_process = file_remaining;
        }

      if(bytes_to_process > block_remaining)
        {
          bytes_to_process = block_remaining;
        }

      bytes_read = fread(
          update.buffer,
          1U,
          bytes_to_process,
          update.file);

      if(bytes_read != bytes_to_process)
        {
          update.error = DWIN_UPDATE_ERROR_FILE_READ;
          return SL_STATUS_IO;
        }

      update.buffer_size = bytes_read;

      return SL_STATUS_OK;
    }

  /*
   * O arquivo terminou
   *
   * O restante do bloco atual deve ser 0x00.
   */
  bytes_to_process = sizeof(update.buffer);

  if(bytes_to_process > block_remaining)
    {
      bytes_to_process = block_remaining;
    }

  memset(update.buffer,
         DWIN_UPDATE_FILL_VALUE,
         bytes_to_process);

  update.buffer_size = bytes_to_process;

  return SL_STATUS_OK;
}

static sl_status_t send_buffer_to_ram(void)
{
  sl_status_t status;
  size_t offset = 0U;

  while (offset < update.buffer_size)
    {
      size_t packet_size =
          update.buffer_size - offset;

      if(packet_size > DWIN_UPDATE_PACKET_SIZE)
        packet_size = DWIN_UPDATE_PACKET_SIZE;

      status = dwin_write(update.ram_address,
                    packet_size,
                    &update.buffer[offset]);
      if(status != SL_STATUS_OK)
        {
          return SL_STATUS_FAIL;
        }

      update.ram_address +=
          (uint16_t)(packet_size / 2U);

      update.block_offset +=
          (uint32_t)packet_size;

      /*
       * Só contabiliza file_offset enquanto
       * estamos enviando dados que pertencem ao arquivo
       */

      if(update.file_offset < update.file_size)
        {
          uint32_t file_remaining = update.file_size - update.file_offset;

          size_t file_bytes = packet_size;

          if(file_bytes > file_remaining)
          {
              file_bytes = file_remaining;
          }

          update.file_offset +=
              (uint32_t)file_bytes;
      }

      offset += packet_size;
  }

  update.buffer_size = 0U;

  return SL_STATUS_OK;
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

      if(dwin_write(update.ram_address, fill_size, zero_buffer) != SL_STATUS_OK)
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

static uint32_t dwin_update_calculate_total_blocks(void)
{
  uint32_t total_ids;

  if(update.method != DWIN_UPDATE_METHOD_0xAA)
    {
      return 0U;
    }

  total_ids = dwin_update_calculate_total_ids();
  return total_ids * DWIN_UPDATE_BLOCKS_PER_FILE_ID_0XAA;
}

static uint32_t dwin_update_calculate_total_ids(void)
{
  return (update.file_size + DWIN_UPDATE_FILE_ID_SIZE - 1U) / DWIN_UPDATE_FILE_ID_SIZE;
}

static sl_status_t dwin_nor_write_block(uint16_t flash_block,
                                        uint16_t ram_address,
                                        uint16_t delay_ms)
{

  uint8_t data[12];

  data[0] = 0x5A;
  data[1] = 0x02;

  data[2] = (uint8_t)(flash_block >> 8);
  data[3] = (uint8_t)(flash_block & 0xFFU);

  data[4] = (uint8_t)(ram_address >> 8);
  data[5] = (uint8_t)(ram_address & 0xFFU);

  data[6] = (uint8_t)(delay_ms >> 8);
  data[7] = (uint8_t)(delay_ms & 0xFFU);

  data[8] = 0x00;
  data[9] = 0x00;
  data[10] = 0x00;
  data[11] = 0x00;

  return dwin_write(
      DWIN_UPDATE_VP_EXTERNAL_FLASH,
      sizeof(data),
      data);
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

static void flash_status_callback(sl_status_t status,
                                  uint16_t vp,
                                  const uint8_t *data,
                                  size_t data_size,
                                  void *context)
{
  (void) vp;
  (void) context;

  update.flash_status_pending = false;
  if(status != SL_STATUS_OK)
    {
      update.error = DWIN_UPDATE_ERROR_DWIN_STATUS;
      update.state = DWIN_UPDATE_STATE_ERROR;
      return;
    }

  if(data == NULL || data_size < 2U)
    {
      update.error = DWIN_UPDATE_ERROR_DWIN_STATUS;
      update.state = DWIN_UPDATE_STATE_ERROR;
      return;
    }

  if(data[0] == 0x00 &&
      data[1] == 0x02)
    {
      update.state = DWIN_UPDATE_STATE_NEXT_BLOCK;
      return;
    }

  if(data[0] == 0x5A &&
      data[1] == 0x02)
    {
      update.state = DWIN_UPDATE_STATE_WAIT_FLASH;
      return;
    }

  update.error = DWIN_UPDATE_ERROR_DWIN_STATUS;
  update.state = DWIN_UPDATE_STATE_ERROR;
}

static void init_update_event(void)
{
  if(update_event_initialized)
    return;

  sl_zigbee_event_init(&dwin_update_event, update_event_handler);

  update_event_initialized = true;
}

static sl_status_t request_flash_status(void)
{
  if(update.method == DWIN_UPDATE_METHOD_0xAA)
    {
      return dwin_read_vp_async(DWIN_UPDATE_VP_EXTERNAL_FLASH,
                                1U,
                                DWIN_UPDATE_FLASH_STATUS_TIMEOUT_MS,
                                flash_status_callback);
    }
  else
    {
      /*
       * Ainda nao implementado o metodo 0x06
       */
    }
  return SL_STATUS_NOT_FOUND;
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

static void dwin_update_case_load_block(sl_status_t *status)
{
  if(update.block_offset >= update.current_block_size)
    {
      uint32_t block_size = dwin_update_get_block_size();

      if(block_size == 0U)
        {
          update.error = DWIN_UPDATE_ERROR_INVALID_FILE;
          update.state = DWIN_UPDATE_STATE_ERROR;
          return;
        }
      if(update.current_block_size == block_size)
        {
          update.state = DWIN_UPDATE_STATE_FLASH_WRITE;
        }
      else
        {
          update.state = DWIN_UPDATE_STATE_FILL_BLOCK;
        }
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
      update.error = DWIN_UPDATE_ERROR_DWIN_WRITE;

      update.state = DWIN_UPDATE_STATE_ERROR;

      return;
    }

  /*
   * Buffer terminou, mas ainda pode estar dentro do bloco.
   */

  if(update.block_offset < update.current_block_size)
    {
      update.state = DWIN_UPDATE_STATE_LOAD_BLOCK;
      return;
    }

  if(update.method == DWIN_UPDATE_METHOD_0xAA)
    {
      if(update.current_block_size == DWIN_UPDATE_FLASH_BLOCK_SIZE_0XAA)
        {
          update.state = DWIN_UPDATE_STATE_FLASH_WRITE;
          return;
        }
    }
  else
    {
      if(update.current_block_size == DWIN_UPDATE_FLASH_BLOCK_SIZE_0X06)
        {
          update.state = DWIN_UPDATE_STATE_FLASH_WRITE;
          return;
        }
    }
  update.state = DWIN_UPDATE_STATE_LOAD_BLOCK;
}

static void dwin_update_case_flash_write(sl_status_t *status)
{
  /*
   * Só é permitido gravar blocos completos nesta primeira versão
   * Ou seja, a atualização ainda não está completa, o último bloco ainda precisa ser implementado
   */

  if(update.method == DWIN_UPDATE_METHOD_0xAA)
    {
      if(update.block_offset != DWIN_UPDATE_FLASH_BLOCK_SIZE_0XAA)
        {
          update.error = DWIN_UPDATE_ERROR_FILE_SIZE;

          update.state = DWIN_UPDATE_STATE_ERROR;

          return;
        }

      *status = dwin_nor_write_block(
          (uint16_t)update.current_block,
          DWIN_UPDATE_RAM_START,
          0U);
    }

  else
    {
      if(update.block_offset != DWIN_UPDATE_FLASH_BLOCK_SIZE_0X06)
        {
          update.error = DWIN_UPDATE_ERROR_FILE_SIZE;

          update.state = DWIN_UPDATE_STATE_ERROR;

          return;
        }

      *status = dwin_os_update_block(
          (uint16_t)update.current_block,
          DWIN_UPDATE_RAM_START,
          0U);
    }



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
    return;

  *status = request_flash_status();

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
      DWIN_UPDATE_FLASH_BLOCK_SIZE_0XAA;

  update.state = DWIN_UPDATE_STATE_LOAD_BLOCK;

  //  update.current_block++;
//
//  /*
//   * Final do arquivo
//   */
//  if(update.file_offset >= update.file_size)
//    {
//      update.state = DWIN_UPDATE_FINISH;
//      return;
//    }
//
//  update.block_offset = 0U;
//  update.ram_address = DWIN_UPDATE_RAM_START;
//
//  update.retry_count = 0U;
//
//  dwin_update_set_current_block_size();
//
//  update.state = DWIN_UPDATE_LOAD_BLOCK;
//
//  return;
}

static void dwin_update_case_update_finish(sl_status_t *status)
{
  if(update.file != NULL)
    {
      dwin_update_close_file();
    }

  update.active = false;

  return;
}

static void dwin_update_case_error(sl_status_t *status)
{
  if(update.file != NULL)
    {
      dwin_update_close_file();
    }

  update.active = false;
  return;
}

bool dwin_update_is_active(void)
{
  return update.active;
}

dwin_update_state_t dwin_update_get_state(void)
{
  return update.state;
}

dwin_update_error_t dwin_update_get_error(void)
{
  return update.error;
}
