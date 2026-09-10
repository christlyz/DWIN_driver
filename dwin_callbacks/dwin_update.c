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
static bool dwin_update_file_id_handler(const char *filename);
static bool dwin_update_file_size_handler(sl_status_t *status);
static void dwin_update_init_values();
static bool get_extension(const char *filename, dwin_update_extension_t *file_extension);
static bool get_file_size(FILE *file, uint32_t *size);
static bool get_file_id(const char *filename, uint8_t *file_id);
static uint32_t file_id_to_flash_block(uint8_t file_id);
static sl_status_t load_next_chunk(void);
static sl_status_t send_buffer_to_ram(void);
static sl_status_t dwin_nor_write_block(uint16_t flash_block,
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
  if(filename == NULL)
    return SL_STATUS_INVALID_PARAMETER;

  if(update.active)
    return SL_STATUS_BUSY;

  memset(&update, 0, sizeof(update));

  update.file = file_ptr;

  if(file_ptr == NULL)
    {
      update.error = DWIN_UPDATE_ERROR_FILE_OPEN;
      return SL_STATUS_FAIL;
    }

  if(!dwin_update_extension_handler(filename))
    {
      return SL_STATUS_INVALID_PARAMETER;
    }

  sl_status_t status;

  if(!dwin_update_file_size_handler(&status))
    {
      return status;
    }

  if(!dwin_update_file_id_handler(filename))
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
      fclose(update.file);
      update.file = NULL;

      update.error = DWIN_UPDATE_ERROR_UNSUPPORTED_FILE;
      return false;
    }
  return true;
}

static bool dwin_update_file_size_handler(sl_status_t *status)
{
  if(!get_file_size(update.file, &update.file_size))
    {
      fclose(update.file);
      update.file = NULL;

      update.error = DWIN_UPDATE_ERROR_FILE_SIZE;
      *status = SL_STATUS_FAIL;
      return false;
    }

  if(update.file_size == 0U || (update.file_size & 1U) != 0U)
    {
      fclose(update.file);
      update.file = NULL;

      update.error = DWIN_UPDATE_ERROR_FILE_SIZE;

      *status = SL_STATUS_INVALID_PARAMETER;
      return false;
    }

  return true;
}

static bool dwin_update_file_id_handler(const char *filename)
{
  if(!get_file_id(filename, &update.id))
    {
      /*
       * Implementar que se não tiver um ID, verificar se não é um arquivo DWIN_OS
       */
      fclose(update.file);
      update.file = NULL;

      update.error = DWIN_UPDATE_ERROR_INVALID_FILE;
      return false;
    }
  return true;
}

static void dwin_update_init_values()
{
  update.current_block = file_id_to_flash_block(update.id);


  update.total_blocks =
      (update.file_size +
          DWIN_UPDATE_FLASH_BLOCK_SIZE - 1U)
          / DWIN_UPDATE_FLASH_BLOCK_SIZE;

  update.file_offset = 0U;
  update.block_offset = 0U;
  update.ram_address = DWIN_UPDATE_RAM_START;
  update.retry_count = 0U;

  uint32_t remaining = update.file_size - update.file_offset;

  if(remaining >= DWIN_UPDATE_FLASH_BLOCK_SIZE)
    {
      update.current_block_size = DWIN_UPDATE_FLASH_BLOCK_SIZE;
    }
  else
    {
      update.current_block_size = remaining;
    }

  update.active = true;
  update.state = DWIN_UPDATE_LOAD_BLOCK;
  update.error = DWIN_UPDATE_ERROR_NONE;
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
  size_t remaining_in_block;
  size_t bytes_to_read;

  remaining_in_block =
      DWIN_UPDATE_FLASH_BLOCK_SIZE - update.block_offset;

  bytes_to_read = remaining_in_block;

  if(bytes_to_read > DWIN_UPDATE_BUFFER_SIZE)
    bytes_to_read = DWIN_UPDATE_BUFFER_SIZE;

  update.buffer_size =
      fread(update.buffer,
            1U,
            bytes_to_read,
            update.file);

  if(update.buffer_size == 0U)
    {
      if(ferror(update.file))
        {
          update.error = DWIN_UPDATE_ERROR_FILE_READ;
          return SL_STATUS_FAIL;
        }

      update.error = DWIN_UPDATE_ERROR_FILE_READ;
      return SL_STATUS_EMPTY;
    }

  if((update.buffer_size & 1U) != 0U)
    {
      update.error = DWIN_UPDATE_ERROR_FILE_SIZE;
      return SL_STATUS_INVALID_PARAMETER;
    }

  return SL_STATUS_OK;
}

static sl_status_t send_buffer_to_ram(void)
{
  size_t offset = 0U;

  while (offset < update.buffer_size)
    {
      size_t chunk_size =
          update.buffer_size - offset;

      if(chunk_size > DWIN_UPDATE_PACKET_SIZE)
        chunk_size = DWIN_UPDATE_PACKET_SIZE;

      if(dwin_write(update.ram_address,
                    chunk_size,
                    &update.buffer[offset]) != SL_STATUS_OK)
        {
          return SL_STATUS_FAIL;
        }

      update.ram_address +=
          (uint16_t)(chunk_size / 2U);

      update.block_offset +=
          (uint32_t)chunk_size;

      update.file_offset +=
          (uint32_t)chunk_size;

      offset += chunk_size;
    }

  return SL_STATUS_OK;
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

static void flash_status_callback(sl_status_t status,
                                  uint16_t vp,
                                  const uint8_t *data,
                                  size_t data_size,
                                  void *context)
{
  if(status != SL_STATUS_OK)
    return;

  if(data == NULL || data_size < 2U)
    {
      update.error = DWIN_UPDATE_ERROR_DWIN_STATUS;
      update.state = DWIN_UPDATE_ERROR;
      return;
    }

  if(data[0] == 0x00 &&
      data[1] == 0x02)
    {
      update.state = DWIN_UPDATE_NEXT_BLOCK;
      return;
    }

  if(data[0] == 0x5A &&
      data[1] == 0x02)
    {
      update.state = DWIN_UPDATE_WAIT_FLASH;
      return;
    }

  update.error = DWIN_UPDATE_ERROR_DWIN_STATUS;
  update.state = DWIN_UPDATE_ERROR;
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
  return dwin_read_vp_async(DWIN_UPDATE_VP_EXTERNAL_FLASH,
                            1U,
                            1000,
                            flash_status_callback);
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
  sl_status_t status;

  if(!update.active)
    return;

  switch(update.state)
  {
    case DWIN_UPDATE_LOAD_BLOCK:
      dwin_update_case_load_block(&status);
      break;

    case DWIN_UPDATE_WRITE_RAM:
      dwin_update_case_write_ram(&status);
      break;

    case DWIN_UPDATE_FLASH_WRITE:
      dwin_update_case_flash_write(&status);
      break;

    case DWIN_UPDATE_WAIT_FLASH:
      dwin_update_case_wait_flash(&status);
      break;

    case DWIN_UPDATE_NEXT_BLOCK:
      dwin_update_case_next_block(&status);
      break;

    case DWIN_UPDATE_FINISH:
      dwin_update_case_update_finish(&status);
      break;

    case DWIN_UPDATE_ERROR:
      dwin_update_case_error(&status);
      break;

    default:
      break;
  }
}

static void dwin_update_case_load_block(sl_status_t *status)
{
  if(update.block_offset == DWIN_UPDATE_FLASH_BLOCK_SIZE)
    {
      update.state = DWIN_UPDATE_FLASH_WRITE;
      return;
    }
  *status = load_next_chunk();

  if(*status != SL_STATUS_OK)
    {
      /*
       * Arquivo terminou antes de completar o bloco.
       *
       * Esse bloco não foi gravado ainda
       */
      if(feof(update.file))
        {
          update.error = DWIN_UPDATE_ERROR_FILE_SIZE;

          update.state = DWIN_UPDATE_ERROR;
        }

      return;
    }

  update.state = DWIN_UPDATE_WRITE_RAM;
}

static void dwin_update_case_write_ram(sl_status_t *status)
{
  *status = send_buffer_to_ram();

  if(*status != SL_STATUS_OK)
    {
      update.error = DWIN_UPDATE_ERROR_DWIN_WRITE;

      update.state = DWIN_UPDATE_ERROR;

      return;
    }

  /*
   * Buffer terminou, mas ainda pode estar dentro do bloco.
   */
  update.state = DWIN_UPDATE_LOAD_BLOCK;
}

static void dwin_update_case_flash_write(sl_status_t *status)
{
  /*
   * Só é permitido gravar blocos completos nesta primeira versão
   */

  if(update.block_offset != DWIN_UPDATE_FLASH_BLOCK_SIZE)
    {
      update.error = DWIN_UPDATE_ERROR_FILE_SIZE;

      update.state = DWIN_UPDATE_ERROR;

      return;
    }

  *status = dwin_nor_write_block(
      (uint16_t)update.current_block,
      DWIN_UPDATE_RAM_START,
      0U);

  if(*status != SL_STATUS_OK)
    {
      update.error = DWIN_UPDATE_ERROR_DWIN_WRITE;

      update.state = DWIN_UPDATE_ERROR;

      return;
    }

  update.state = DWIN_UPDATE_WAIT_FLASH;
}

static void dwin_update_case_wait_flash(sl_status_t *status)
{
  *status = request_flash_status();

  if(*status != SL_STATUS_OK)
    {
      update.error = DWIN_UPDATE_ERROR_DWIN_STATUS;

      update.state = DWIN_UPDATE_ERROR;
    }

  /*
   * O callback decide:
   *
   * 5A02 -> WAIT_FLASH
   * 0002 -> NEXT_BLOCK
   */

  return;
}

static void dwin_update_case_next_block(sl_status_t *status)
{
  update.current_block++;

  /*
   * Final do arquivo
   */
  if(update.file_offset >= update.file_size)
    {
      update.state = DWIN_UPDATE_FINISH;
      return;
    }

  update.block_offset = 0U;
  update.ram_address = DWIN_UPDATE_RAM_START;

  update.retry_count = 0U;

  update.state = DWIN_UPDATE_LOAD_BLOCK;
  return;
}

static void dwin_update_case_update_finish(sl_status_t *status)
{
  if(update.file != NULL)
    {
      fclose(update.file);
      update.file = NULL;
    }

  update.active = false;

  return;
}

static void dwin_update_case_error(sl_status_t *status)
{
  if(update.file != NULL)
    {
      fclose(update.file);
      update.file = NULL;
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
