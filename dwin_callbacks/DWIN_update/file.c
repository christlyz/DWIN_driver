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
#include "file.h"
#include "zigbee_app_framework_event.h"
#include <stdio.h>
/*******************************************************************************
 * Data types
 ******************************************************************************/
typedef struct file_node
{
  dwin_update_file_t *file;
  struct file_node *next;
} file_node_t;

static file_node_t *file_list = NULL;
static file_node_t *current_node = NULL;

static bool finished = false;
static uint8_t progress = 0;
static uint8_t files_quantity = 0;
static uint8_t files_updated = 0;
static files_update_state_t state;

static sl_zigbee_event_t progress_event;
static void progress_handler(sl_zigbee_event_t *event);
static sl_status_t file_list_build(void);
/*******************************************************************************
 * Extern
 ******************************************************************************/

/*******************************************************************************
 * Private Function Prototypes
 ******************************************************************************/
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
 * Responsável por executar uma etapa da atualização dos arquivos,
 * iniciando cada arquivo sequencialmente e realizando o reset somente
 * depois que todos os arquivos forem concluídos.
 */
sl_status_t start_update()
{
  sl_status_t status;

  switch(state)
  {
    case FILE_UPDATE_STATE_INIT:
      status = file_list_build();

      if(status != SL_STATUS_OK)
        {
          state = FILE_UPDATE_STATE_ERROR;
          return status;
        }

      printf("Quantidade de arquivos: %u\r\n", files_quantity);
      current_node = file_list;
      finished = false;

      sl_zigbee_event_init(&progress_event, progress_handler);

      state = FILE_UPDATE_STATE_START_FILE;

      return SL_STATUS_IS_WAITING;
      break;
    case FILE_UPDATE_STATE_START_FILE:
      if(current_node == NULL)
        {
          state = FILE_UPDATE_STATE_RESET;
          return SL_STATUS_IS_WAITING;
        }

      printf("Iniciando arquivo: %s\r\n", current_node->file->name);

      sl_zigbee_event_set_delay_ms(&progress_event, 0U);

      finished = false;

      status = dwin_update_start(current_node->file, &finished);

      if(status != SL_STATUS_OK)
        {
          state = FILE_UPDATE_STATE_ERROR;
          return status;
        }

      state = FILE_UPDATE_STATE_WAIT_FILE;

      return SL_STATUS_IS_WAITING;
      break;
    case FILE_UPDATE_STATE_WAIT_FILE:
      if(finished)
        {
          state = FILE_UPDATE_STATE_NEXT_FILE;
          return SL_STATUS_IS_WAITING;
        }

      /*
       * Se a atualização deixou de estar ativa sem sinalizar finished, ocorreu um erro.
       */
      if(!dwin_update_is_active())
        {
          state = FILE_UPDATE_STATE_ERROR;
          return SL_STATUS_FAIL;
        }

      return SL_STATUS_IS_WAITING;
      break;
    case FILE_UPDATE_STATE_NEXT_FILE:
      files_updated++;
      printf("Arquivo concluido: %s (%u/%u)\r\n", current_node->file->name, files_updated, files_quantity);
      current_node = current_node->next;
      finished = false;

      if(current_node == NULL)
        {
          state = FILE_UPDATE_STATE_RESET;
        }
      else
        {
          state = FILE_UPDATE_STATE_START_FILE;
        }

      return SL_STATUS_IS_WAITING;
      break;
    case FILE_UPDATE_STATE_RESET:
      printf("Todos os arquivos foram atualizados.\r\n");

      dwin_reset();

      state = FILE_UPDATE_STATE_FINISH;

      return SL_STATUS_IS_WAITING;
      break;
    case FILE_UPDATE_STATE_FINISH:
      sl_zigbee_event_set_inactive(&progress_event);

      file_list_clear();

      finished = false;

      state = FILE_UPDATE_STATE_INIT;

      return SL_STATUS_OK;
      break;
    case FILE_UPDATE_STATE_ERROR:
      printf("Erro durante atualizacao.\r\n");

      sl_zigbee_event_set_inactive(&progress_event);

      file_list_clear();

      finished = false;

      state = FILE_UPDATE_STATE_INIT;

      return SL_STATUS_FAIL;
      break;
    default:
      state = FILE_UPDATE_STATE_ERROR;
      return SL_STATUS_IS_WAITING;
      break;
  }
}

static void progress_handler(sl_zigbee_event_t *event)
{
  (void)event;

  progress = dwin_update_get_progress();
  printf("Atualizacao do arquivo: %u%%\r\n", progress);

  if(state != FILE_UPDATE_STATE_FINISH &&
      state != FILE_UPDATE_STATE_ERROR)
    {
      sl_zigbee_event_set_delay_ms(&progress_event, 500);
    }

}

/*
 * Responsável por adicionar um arquivo ao final da lista de arquivos que serão atualizados.
 */
sl_status_t file_list_add(dwin_update_file_t *file)
{
  file_node_t *new_node;
  file_node_t *node;

  if(file == NULL)
    {
      return SL_STATUS_NULL_POINTER;
    }

  new_node = malloc(sizeof(file_node_t));

  if(new_node == NULL)
    {
      return SL_STATUS_ALLOCATION_FAILED;
    }

  new_node->file = file;
  new_node->next = NULL;

  files_quantity++;
  if(file_list == NULL)
    {
      file_list = new_node;
      return SL_STATUS_OK;
    }

  node = file_list;

  while(node->next != NULL)
    {
      node = node->next;
    }

  node->next = new_node;

  return SL_STATUS_OK;
}

/*
 * Responsável por liberar todos os nós da lista de arquivos.
 */
void file_list_clear(void)
{
  file_node_t *node;
  file_node_t *next;

  node = file_list;

  while(node != NULL)
    {
      next = node->next;

      free(node);

      node = next;
    }

  file_list = NULL;
  files_quantity = 0;
  current_node = NULL;
}

/*
 * Responsável por registrar todos os arquivos disponíveis para a atualização
 */
static sl_status_t file_list_build(void)
{
  sl_status_t status;

  status = file_list_add(get_file_13());

  if(status != SL_STATUS_OK)
    {
      return status;
    }

  status = file_list_add(get_file_14());

  if(status != SL_STATUS_OK)
    {
      return status;
    }

  status = file_list_add(get_file_22());

  if(status != SL_STATUS_OK)
    {
      return status;
    }

  status = file_list_add(get_file_32());

  if(status != SL_STATUS_OK)
    {
      return status;
    }
  return status;
}
