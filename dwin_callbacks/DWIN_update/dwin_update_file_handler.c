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
#include "dwin_update_file_handler.h"
#include <string.h>
#include <ctype.h>
/*******************************************************************************
 * Data types
 ******************************************************************************/

/*******************************************************************************
 * Extern
 ******************************************************************************/

/*******************************************************************************
 * Private Function Prototypes
 ******************************************************************************/
static bool dwin_update_file_get_extension(const char *filename, dwin_update_extension_t *extension);
static bool dwin_update_file_get_id(const char *filename, uint8_t *file_id);
static uint32_t file_id_to_flash_block(uint8_t file_id);
static uint32_t dwin_update_calculate_total_blocks(dwin_update_t *update);
static uint32_t dwin_update_calculate_total_ids(dwin_update_t *update);
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
/*
 * Responsável por identificar a extensão do arquivo e armazená-la
 * na estrutura da atualização.
 */
bool dwin_update_extension_handler(dwin_update_t *update)
{
  if(!dwin_update_file_get_extension(update->file->name, &update->extension))
    {
      update->error = DWIN_UPDATE_ERROR_UNSUPPORTED_FILE;
      return false;
    }
  return true;
}

/*
 * Responsável por validar o tamanho do arquivo e calcular os valores
 * necessários para sua distribuição na memória.
 */
bool dwin_update_file_size_handler(sl_status_t *status, dwin_update_t *update)
{
  if(update->file->size == 0U)
    {
      update->error = DWIN_UPDATE_ERROR_FILE_EMPTY;
      *status = SL_STATUS_INVALID_PARAMETER;
      return false;
    }

  if((update->file->size & 1U) != 0U)
    {
      update->error = DWIN_UPDATE_ERROR_FILE_SIZE;
      *status = SL_STATUS_INVALID_PARAMETER;
      return false;
    }

  update->file_size = (uint32_t)update->file->size;

  *status = SL_STATUS_OK;
  return true;
}

/*
 * Responsável por determinar o método de atualização com base
 * no nome e no tipo do arquivo.
 */
bool dwin_update_identify_file_handler(dwin_update_t *update)
{
  if(dwin_update_file_get_id(update->file->name, &update->id))
    {
      update->method = DWIN_UPDATE_METHOD_0xAA;
      return true;
    }

  if(update->extension == DWIN_UPDATE_EXTENSION_BIN &&
      strncmp(update->file->name, "DWINOS", 6U) == 0)
    {
      update->method = DWIN_UPDATE_METHOD_0x06;
      return true;
    }

  update->method = DWIN_UPDATE_METHOD_INVALID;
  update->error = DWIN_UPDATE_ERROR_INVALID_FILE;
  return false;
}

/*
 * Responsável por verificar se todos os IDs físicos necessários
 * pelo arquivo estão dentro da faixa suportada.
 */
bool dwin_update_validate_file_id_range(dwin_update_t *update)
{
  uint32_t total_ids;
  uint32_t last_id;

  if(update->method != DWIN_UPDATE_METHOD_0xAA)
    {
      return true;
    }

  total_ids = dwin_update_calculate_total_ids(update);

  last_id = (uint32_t)update->id + total_ids - 1U;

  if(last_id > 63U)
    {
      update->error = DWIN_UPDATE_ERROR_INVALID_FILE;
      return false;
    }

  return true;
}

/*
 * Responsável por inicializar os offsets, endereço RAM, bloco inicial,
 * tamanho do bloco e demais parâmetros da máquina de estados.
 */
void dwin_update_init_values(dwin_update_t *update)
{
  update->file_offset = 0U;
  update->block_offset = 0U;
  update->block_file_offset = 0U;
  update->buffer_size = 0U;
  update->ram_address = DWIN_UPDATE_RAM_START;
  update->progress = 0U;
  update->flash_status_pending = false;
  update->ram_retry_count = 0U;
  update->flash_retry_count = 0U;
  update->flash_status_retry_count = 0U;

  if(update->method == DWIN_UPDATE_METHOD_0xAA)
    {
      update->file_id_base_block = file_id_to_flash_block(update->id);
      update->current_block = file_id_to_flash_block(update->id);

      /*
       * Quantidade total de blocos físicos necessários, incluindo o preenchimento do último ID.
       */
      update->total_blocks = dwin_update_calculate_total_blocks(update);
      update->current_block_size = DWIN_UPDATE_FLASH_BLOCK_SIZE_0XAA;
    }
  else
    {
      update->file_id_base_block = 0U;
      update->current_block = 0U;
      update->total_blocks = 0U;
      update->current_block_size = 0U;
    }

  update->active = true;
  update->state = DWIN_UPDATE_STATE_ENABLE_CRC;
  update->error = DWIN_UPDATE_ERROR_NONE;
}

/*
 * Responsável por extrair a extensão do nome do arquivo e convertê-la
 * para o tipo de extensão utilizado pela atualização.
 */
static bool dwin_update_file_get_extension(const char *filename, dwin_update_extension_t *file_extension)
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

/*
 * Responsável por extrair a extensão do nome do arquivo e convertê-la
 * para o tipo de extensão utilizado pela atualização.
 */
static bool dwin_update_file_get_id(const char *filename, uint8_t *file_id)
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

/*
 * Responsável por converter um ID lógico de arquivo no endereço do
 * primeiro bloco físico correspondente na Flash.
 */
static uint32_t file_id_to_flash_block(uint8_t file_id)
{
  return (uint32_t)file_id * 8U;
}

/*
 * Responsável por calcular quantos IDs lógicos são necessários para
 * armazenar o arquivo.
 */
static uint32_t dwin_update_calculate_total_ids(dwin_update_t *update)
{
  return (update->file_size + DWIN_UPDATE_FILE_ID_SIZE - 1U) / DWIN_UPDATE_FILE_ID_SIZE;
}

/*
 * Responsável por calcular quantos blocos físicos serão necessários
 * para armazenar o arquivo.
 */
static uint32_t dwin_update_calculate_total_blocks(dwin_update_t *update)
{
  uint32_t total_ids;

  if(update->method != DWIN_UPDATE_METHOD_0xAA)
    {
      return 0U;
    }

  total_ids = dwin_update_calculate_total_ids(update);
  return total_ids * DWIN_UPDATE_BLOCKS_PER_FILE_ID_0XAA;
}
