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
#include "dwin_update_internal.h"
#include "../DWIN_functions/dwin_service.h"
#include "zigbee_app_framework_event.h"

#include "dwin_update_file_handler.h"
#include "dwin_update_transfer.h"
#include "dwin_update_flash.h"

#include <stdio.h>
#include <string.h>
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
static uint32_t dwin_update_get_block_size(void);
//static sl_status_t dwin_os_update_block(uint16_t flash_block,
//                                        uint16_t ram_address,
//                                        uint16_t delay_ms);
static void init_update_event(void);
static void update_event_handler(sl_zigbee_event_t *event);
static void dwin_update_process(void);
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

/*
 * Responsável por inicializar o evento responsável pelo processamento
 * periódico da atualização.
 */
static void init_update_event(void)
{
  if(update_event_initialized)
    return;

  sl_zigbee_event_init(&dwin_update_event, update_event_handler);

  update_event_initialized = true;
}

/*
 * Responsável por executar uma iteração da atualização através
 * do evento do Zigbee.
 */
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
      sl_zigbee_event_set_inactive(&dwin_update_event);
    }
}

/*
 * Responsável por iniciar uma nova atualização.
 */
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

/*
 * Responsável por executar a máquina de estados da atualização.
 */
static void dwin_update_process(void)
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

/*
 * Responsável por habilitar o CRC antes do início da transferência.
 */
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

/*
 * Responsável por aguardar a confirmação de que o CRC foi habilitado.
 */
static void dwin_update_case_wait_crc_enables(sl_status_t *status)
{
  if(dwin_is_crc_enabled())
    {
      update.state = DWIN_UPDATE_STATE_LOAD_BLOCK;
    }
}

/*
 * Responsável por preparar o próximo trecho do arquivo para transferência.
 */
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

/*
 * Responsável por iniciar o envio de um trecho da RAM para a DWIN.
 */
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

/*
 * Responsável por aguardar o ACK da escrita do trecho na RAM.
 */
static void dwin_update_case_wait_ram_ack(sl_status_t *status)
{
  (void) status;
}

/*
 * Responsável por iniciar a gravação do bloco na Flash externa.
 */
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

/*
 * Responsável por aguardar e consultar o status da gravação na Flash.
 */
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

/*
 * Responsável por finalizar o bloco atual e preparar o próximo bloco.
 */
static void dwin_update_case_next_block(sl_status_t *status)
{
  uint32_t blocks_completed;

  (void) status;

  /*
   * current_block é baseado em um endereço absoluto
   * do Flash.
   */
  blocks_completed =
      update.current_block -
      update.file_id_base_block;

  printf("Bloco %ld completo\r\n", blocks_completed);

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

/*
 * Responsável por desabilitar o CRC após a atualização.
 */
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

/*
 * Responsável por aguardar a confirmação de que o CRC foi desabilitado.
 */
static void dwin_update_case_wait_crc_disable(sl_status_t *status)
{
  (void) status;

  if(dwin_is_crc_enabled())
    {
      return;
    }

  update.state = DWIN_UPDATE_STATE_FINISH;
}

/*
 * Responsável por finalizar uma atualização que terminou com erro
 * depois que o CRC foi desabilitado.
 */
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

/*
 * Responsável por finalizar uma atualização concluída com sucesso.
 */
static void dwin_update_case_update_finish(sl_status_t *status)
{
  (void)status;

  if(update.file != NULL)
    {
      dwin_update_file_close(update.file);
    }

  update.active = false;
  if(finished != NULL)
    {
      *finished = true;
    }
}

/*
 * Responsável por tratar o encerramento da atualização devido a erro.
 */
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

/*
 * Responsável por verificar se um erro pode ser recuperado através
 * de uma nova tentativa.
 */
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

/*
 * Responsável por informar se existe uma atualização em andamento.
 */
bool dwin_update_is_active(void)
{
  return update.active;
}

/*
 * Responsável por calcular o progresso atual da atualização.
 */
void dwin_update_calculate_progress(void)
{
  uint64_t completed_bytes;
  uint64_t total_bytes;
  uint32_t block_size;

  block_size = dwin_update_get_block_size();

  if(update.total_blocks == 0U ||
      block_size == 0U)
    {
      update.progress = 0U;
      return;
    }

  total_bytes = (uint64_t)update.total_blocks * (uint64_t)block_size;

  completed_bytes = (uint64_t)(update.current_block - update.file_id_base_block) * (uint64_t)block_size;

  completed_bytes += (uint64_t)update.block_offset;

  if(completed_bytes >= total_bytes)
    {
      update.progress = 100U;
      return;
    }

  update.progress = (uint8_t)((completed_bytes * 100U) / total_bytes);
}

/*
 * Responsável por retornar o progresso atual da atualização.
 */
uint8_t dwin_update_get_progress(void)
{
  return update.progress;
}

/*
 * Responsável por retornar o estado atual da atualização.
 */
dwin_update_state_t dwin_update_get_state(void)
{
  return update.state;
}

/*
 * Responsável por retornar o último erro da atualização.
 */
dwin_update_error_t dwin_update_get_error(void)
{
  return update.error;
}

/*
 * Responsável por imprimir informações de diagnóstico da atualização.
 */
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
