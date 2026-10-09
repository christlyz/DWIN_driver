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
#include "sl_sleeptimer.h"
#include <stdio.h>
/*******************************************************************************
 * Data types
 ******************************************************************************/
static file_node_t *version_node = NULL;
static dwin_update_file_t *version_file = NULL;
static uint8_t installed_version_data[GS_VERSION_SIZE_BYTES];
static uint8_t available_version_data[GS_VERSION_SIZE_BYTES];
static size_t installed_version_data_size = 0U;
static bool version_read_done = false;
static sl_status_t version_read_status = SL_STATUS_OK;
static bool version_wait_done = false;
static uint16_t update_files_quantity = 0U;
static gs_version_t installed_version;
static gs_version_t available_version;
static sl_zigbee_event_t version_wait_event;
static void version_wait_handler(sl_zigbee_event_t *event);

file_node_t *file_list;
static file_node_t *current_node = NULL;

static bool finished = false;
static uint8_t progress = 0;
static uint8_t files_quantity = 0;
static uint8_t files_updated = 0;
static files_update_state_t state;

static uint64_t update_start_tick = 0U;
static uint64_t update_time_ms = 0U;

static sl_zigbee_event_t progress_event;
static void progress_handler(sl_zigbee_event_t *event);

static bool force_update = false;
/*******************************************************************************
 * Extern
 ******************************************************************************/

/*******************************************************************************
 * Private Function Prototypes
 ******************************************************************************/
static void file_update_case_init(sl_status_t *status);
static void file_update_case_find_version(sl_status_t *status);
static void file_update_case_read_version_nor(sl_status_t *status);
static void file_update_case_wait_version_nor(sl_status_t *status);
static void file_update_case_read_version_vp(sl_status_t *status);
static void file_update_case_wait_version_vp(sl_status_t *status);
static void file_update_case_compare_version(sl_status_t *status);
static void file_update_case_start_file(sl_status_t *status);
static void file_update_case_wait_file(sl_status_t *status);
static void file_update_case_next_file(sl_status_t *status);
static void file_update_case_save_version(sl_status_t *status);
static void file_update_case_wait_version_save(sl_status_t *status);
static void file_update_case_no_update(sl_status_t *status);
static void file_update_case_reset(sl_status_t *status);
static void file_update_case_recovery(sl_status_t *status);
static void file_update_case_finish(sl_status_t *status);
static void file_update_case_error(sl_status_t *status);

static file_node_t *find_version_node(void);
static file_node_t *find_next_update_file(file_node_t *node);
static uint16_t count_update_files(void);
static void version_read_callback(sl_status_t status, uint16_t vp, const uint8_t *data, size_t data_size, void *context);
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
void file_update_set_force(bool force)
{
  force_update = force;
}

static void version_wait_handler(sl_zigbee_event_t *event)
{
  version_wait_done = true;

  sl_zigbee_event_set_inactive(event);
}

void file_update_init(void)
{
  sl_zigbee_event_init(&progress_event, progress_handler);
  sl_zigbee_event_init(&version_wait_event, version_wait_handler);
}

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
      file_update_case_init(&status);
      break;

    case FILE_UPDATE_STATE_FIND_VERSION:
      file_update_case_find_version(&status);
      break;

    case FILE_UPDATE_STATE_READ_VERSION_NOR:
      file_update_case_read_version_nor(&status);
      break;

    case FILE_UPDATE_STATE_WAIT_VERSION_NOR:
      file_update_case_wait_version_nor(&status);
      break;

    case FILE_UPDATE_STATE_READ_VERSION_VP:
      file_update_case_read_version_vp(&status);
      break;

    case FILE_UPDATE_STATE_WAIT_VERSION_VP:
      file_update_case_wait_version_vp(&status);
      break;

    case FILE_UPDATE_STATE_COMPARE_VERSION:
      file_update_case_compare_version(&status);
      break;

    case FILE_UPDATE_STATE_START_FILE:
      file_update_case_start_file(&status);
      break;

    case FILE_UPDATE_STATE_WAIT_FILE:
      file_update_case_wait_file(&status);
      break;

    case FILE_UPDATE_STATE_NEXT_FILE:
      file_update_case_next_file(&status);
      break;

    case FILE_UPDATE_STATE_SAVE_VERSION:
      file_update_case_save_version(&status);
      break;

    case FILE_UPDATE_STATE_WAIT_VERSION_SAVE:
      file_update_case_wait_version_save(&status);
      break;

    case FILE_UPDATE_STATE_NO_UPDATE:
      file_update_case_no_update(&status);
      break;

    case FILE_UPDATE_STATE_RESET:
      file_update_case_reset(&status);
      break;

    case FILE_UPDATE_STATE_RECOVERY:
      file_update_case_recovery(&status);
      break;

    case FILE_UPDATE_STATE_FINISH:
      file_update_case_finish(&status);
      break;

    case FILE_UPDATE_STATE_ERROR:
      file_update_case_error(&status);
      break;

    default:
      state = FILE_UPDATE_STATE_ERROR;
      status = SL_STATUS_IS_WAITING;
      break;
  }
  return status;
}

static void progress_handler(sl_zigbee_event_t *event)
{
  (void)event;

  progress = dwin_update_get_progress();
  printf("Atualizacao do arquivo: %u%%\r\n", progress);

  if((state != FILE_UPDATE_STATE_FINISH) &&
      (state != FILE_UPDATE_STATE_ERROR) &&
      (state != FILE_UPDATE_STATE_RECOVERY) &&
      (state != FILE_UPDATE_STATE_NO_UPDATE))
    {
      sl_zigbee_event_set_delay_ms(&progress_event, 500U);
    }

}

static void file_update_case_init(sl_status_t *status)
{
  *status = file_list_build();

  if(*status != SL_STATUS_OK)
    {
      state = FILE_UPDATE_STATE_ERROR;
      return;
    }

  printf("Quantidade total de arquivos: %u\r\n", files_quantity);

  /*
   * Procura gs_version.txt independentemente da posição dentro da lista.
   */
  state = FILE_UPDATE_STATE_FIND_VERSION;

  *status = SL_STATUS_IS_WAITING;
}

static void file_update_case_find_version(sl_status_t *status)
{
  version_node = find_version_node();

  if(version_node == NULL)
    {
      printf("Arquivo de versao nao encontrado.\r\n");

      state = FILE_UPDATE_STATE_ERROR;
      *status = SL_STATUS_NOT_FOUND;
      return;
    }

  version_file = version_node->file;

  update_files_quantity = count_update_files();

  printf("Arquivo de versao encontrado: %s\r\n", version_file->name);

  printf("Quantidade de arquivos de atualizacao: %u\r\n", update_files_quantity);

  /*
   * Comeca a leitura da versao atualmente instalada.
   */
  state = FILE_UPDATE_STATE_READ_VERSION_NOR;

  *status = SL_STATUS_IS_WAITING;
}

static void file_update_case_read_version_nor(sl_status_t *status)
{
  printf("Lendo versao atual da NOR Flash...\r\n");
  *status = dwin_read_version_nor_flash();

  if(*status != SL_STATUS_OK)
    {
      printf("Erro ao iniciar leitura da NOR Flash: 0x%08lx\r\n", (unsigned long)*status);

      state = FILE_UPDATE_STATE_ERROR;
      return;
    }

  /*
   * A operacao do VP 0x08 precisa de aproximadamente 100ms para concluir.
   */
  version_wait_done = false;

  sl_zigbee_event_set_delay_ms(&version_wait_event, 100U);

  state = FILE_UPDATE_STATE_WAIT_VERSION_NOR;

  *status = SL_STATUS_IS_WAITING;
}

static void file_update_case_wait_version_nor(sl_status_t *status)
{
  if(!version_wait_done)
    {
      *status = SL_STATUS_IS_WAITING;
      return;
    }

  state = FILE_UPDATE_STATE_READ_VERSION_VP;

  *status = SL_STATUS_IS_WAITING;
}

static void file_update_case_read_version_vp(sl_status_t *status)
{
  printf("Lendo versao do VP 0x%04X...\r\n", DWIN_VP_VERSION);

  version_read_done = false;
  version_read_status = SL_STATUS_OK;
  installed_version_data_size = 0U;

  *status = dwin_read_vp_async(DWIN_VP_VERSION,
                              GS_VERSION_SIZE_WORDS,
                              1000,
                              version_read_callback);

  if(*status != SL_STATUS_OK)
    {
      printf("Erro ao iniciar leitura da versao: 0x%08lx\r\n", (unsigned long)*status);

      state = FILE_UPDATE_STATE_ERROR;
      return;
    }

  state = FILE_UPDATE_STATE_WAIT_VERSION_VP;

  *status = SL_STATUS_IS_WAITING;
}

static void file_update_case_wait_version_vp(sl_status_t *status)
{
  if(!version_read_done)
    {
      *status = SL_STATUS_IS_WAITING;
      return;
    }

  if(version_read_status != SL_STATUS_OK)
    {
      printf("Erro ao recuperar versao da DWIN: 0x%08lx\r\n", (unsigned long)version_read_status);

      state = FILE_UPDATE_STATE_ERROR;
      *status = version_read_status;
      return;
    }

  state = FILE_UPDATE_STATE_COMPARE_VERSION;

  *status = SL_STATUS_IS_WAITING;
}

static void file_update_case_compare_version(sl_status_t *status)
{
  int8_t comparison;
  bool force_current_update;

  if((version_file == NULL) || (version_file->data == NULL) || (version_file->size != GS_VERSION_FILE_SIZE_BYTES))
    {
      printf("Arquivo de versao invalido.\r\n");

      state = FILE_UPDATE_STATE_ERROR;
      *status = SL_STATUS_INVALID_PARAMETER;
      return;
    }

  memset(available_version_data, ' ', sizeof(available_version_data));

  memcpy(available_version_data, version_file->data, version_file->size);

  /*
   * Versao que veio na nova atualizacao.
   */
  *status = gs_version_parse(available_version_data, sizeof(available_version_data), &available_version);

  if(*status != SL_STATUS_OK)
    {
      printf("Formato da nova versao invalido.\r\n");

      state = FILE_UPDATE_STATE_ERROR;
      return;
    }

  /*
   * Tenta interpretar a versao atualmente instalada.
   */
  *status = gs_version_parse(installed_version_data, installed_version_data_size, &installed_version);

  if(*status != SL_STATUS_OK)
    {
      /*
       * NOR ainda sem uma versao valida.
       *
       * Considera 0.0.0 para permitir a primeira atualizacao.
       */
      installed_version.major = 0U;
      installed_version.minor = 0U;

      printf("Versao armazenada na NOR invalida.\r\n");
      printf("Considerando versao instalada como EFAS_00.00\r\n");
    }

  printf("Versao instalada: EFAS_%02u.%02u\r\n", installed_version.major, installed_version.minor);
  printf("Versao disponivel: EFAS_%02u.%02u\r\n", available_version.major, available_version.minor);

  comparison = gs_version_compare(&available_version, &installed_version);

  force_current_update = force_update;
  force_update = false;
  if((comparison <= 0) && !force_current_update)
    {
      printf("Atualizacao nao necessaria.\r\n");

      state = FILE_UPDATE_STATE_NO_UPDATE;
      *status = SL_STATUS_IS_WAITING;
      return;
    }

  if((comparison <= 0) && force_current_update)
    {
      printf("Atualizacao forcada!\r\n");
      printf("A versao disponivel e igual ou inferior a instalada.\r\n");
    }
  else
    {
      printf("Nova versao detectada. Iniciando atualizacao.\r\n");
    }

  /*
   * Ignora gs_version.txt durante a atualizacao.
   */
  current_node =  find_next_update_file(file_list);

  files_updated = 0U;

  finished = false;

  if(current_node == NULL)
    {
      /*
       * Nao existem arquivos reais para atualizar.
       */
      printf("Nenhum arquivo para atualizar\r\n");
      state = FILE_UPDATE_STATE_NO_UPDATE;
    }
  else
    {
      state = FILE_UPDATE_STATE_START_FILE;
    }

  *status = SL_STATUS_IS_WAITING;
}

static void file_update_case_start_file(sl_status_t *status)
{
  if(current_node == NULL)
    {
      state = FILE_UPDATE_STATE_SAVE_VERSION;
      *status = SL_STATUS_IS_WAITING;
      return;
    }

  /*
   * O primeiro arquivo real inicia a medicao do tempo.
   */
  if(files_updated == 0U)
    {
      update_start_tick = sl_sleeptimer_get_tick_count64();
    }

  printf("Iniciando arquivo: %s\r\n", current_node->file->name);

  progress = 0U;

  sl_zigbee_event_set_delay_ms(&progress_event, 0U);

  finished = false;

  *status = dwin_update_start(current_node->file, &finished);

  if(*status != SL_STATUS_OK)
    {
      printf("Erro ao iniciar arquivo: 0x%08lx\r\n", (unsigned long) *status);

      state = FILE_UPDATE_STATE_ERROR;
      return;
    }

  state = FILE_UPDATE_STATE_WAIT_FILE;

  *status = SL_STATUS_IS_WAITING;
}

static void file_update_case_wait_file(sl_status_t *status)
{
  if(finished)
    {
      state = FILE_UPDATE_STATE_NEXT_FILE;
      *status = SL_STATUS_IS_WAITING;
      return;
    }

  /*
   * Se deixou de estar ativo sem indicar finished, ocorreu um erro.
   */
  if(!dwin_update_is_active())
    {
      printf("Atualizacao do arquivo terminou com erro.\r\n");

      state = FILE_UPDATE_STATE_ERROR;
      *status = SL_STATUS_FAIL;
      return;
    }

  *status = SL_STATUS_IS_WAITING;
}

static void file_update_case_next_file(sl_status_t *status)
{
  files_updated++;

  printf("Arquivo concluido: %s (%u/%u)\r\n",
         current_node->file->name,
         files_updated,
         update_files_quantity);

  /*
   * Procura o proximo arquivo real.
   * gs_version.txt sera ignorado.
   */
  current_node = find_next_update_file(current_node->next);

  finished = false;

  if(current_node == NULL)
    {
      /*
       * TODOS os arquivos foram atualizados.
       *
       * Somente agora podemos registrar a nova versao.
       */
      state = FILE_UPDATE_STATE_SAVE_VERSION;
    }
  else
    {
      state = FILE_UPDATE_STATE_START_FILE;
    }

  *status = SL_STATUS_IS_WAITING;
}

static void file_update_case_save_version(sl_status_t *status)
{
  uint8_t data[GS_VERSION_SIZE_BYTES];

  sl_zigbee_event_set_inactive(&progress_event);

  printf("Todos os arquivos foram atualizados.\r\n");
  printf("Salvando nova versao na NOR Flash...\r\n");

  if((version_file == NULL) ||
     (version_file->data == NULL) ||
     (version_file->size != GS_VERSION_FILE_SIZE_BYTES))
  {
    printf("Arquivo de versao invalido para gravacao.\r\n");

    state = FILE_UPDATE_STATE_ERROR;
    *status = SL_STATUS_INVALID_PARAMETER;
    return;
  }

  /*
   * Preenche os 12 bytes com espacos.
   */
  memset(data, ' ', sizeof(data));

  /*
   * Copia os 10 bytes do arquivo.
   * Os bytes 10 e 11 continuam como 0x20.
   */
  memcpy(data, version_file->data, version_file->size);

  /*
   * Escreve os 12 bytes no VP 0x6000.
   */
  *status = dwin_write(DWIN_VP_VERSION,
                       data,
                       sizeof(data));

  if(*status != SL_STATUS_OK)
  {
    printf("Erro ao gravar versao no VP: 0x%08lx\r\n",
           (unsigned long)*status);

    state = FILE_UPDATE_STATE_ERROR;
    return;
  }

  /*
   * Copia o VP para a NOR Flash.
   */
  *status = dwin_write_version_nor_flash();

  if(*status != SL_STATUS_OK)
  {
    printf("Erro ao gravar versao na NOR Flash: 0x%08lx\r\n",
           (unsigned long)*status);

    state = FILE_UPDATE_STATE_ERROR;
    return;
  }

  /*
   * Aguarda a operacao da NOR Flash.
   */
  version_wait_done = false;

  sl_zigbee_event_set_delay_ms(&version_wait_event, 100U);

  state = FILE_UPDATE_STATE_WAIT_VERSION_SAVE;

  *status = SL_STATUS_IS_WAITING;
}

static void file_update_case_wait_version_save(sl_status_t *status)
{
  if(!version_wait_done)
    {
      *status = SL_STATUS_IS_WAITING;
      return;
    }

  printf("Nova versao salva na NOR Flash.\r\n");

  state = FILE_UPDATE_STATE_RESET;

  *status = SL_STATUS_IS_WAITING;
}

static void file_update_case_no_update(sl_status_t *status)
{
  printf("Nenhuma atualizacao sera realizada.\r\n");

  sl_zigbee_event_set_inactive(&progress_event);

  state = FILE_UPDATE_STATE_FINISH;

  *status = SL_STATUS_IS_WAITING;
}

static void file_update_case_reset(sl_status_t *status)
{
  uint64_t elapsed_ticks;
  uint64_t minutes;
  uint64_t seconds;
  uint64_t milliseconds;
  sl_status_t time_status;

  elapsed_ticks = sl_sleeptimer_get_tick_count64() - update_start_tick;
  time_status = sl_sleeptimer_tick64_to_ms(elapsed_ticks, &update_time_ms);

  minutes = update_time_ms / 60000U;
  seconds = (update_time_ms % 60000U) / 1000U;
  milliseconds = update_time_ms % 1000U;

  if(time_status == SL_STATUS_OK)
    {
      printf("Tempo total: %llu min %02llu s %03llu ms\r\n", (unsigned long long)minutes, (unsigned long long)seconds,(unsigned long long)milliseconds);
    }
  else
    {
      printf("Erro ao calcular tempo de atualizacao.\r\n");
    }

  printf("Reiniciando DWIN.\r\n");

  dwin_reset();

  state = FILE_UPDATE_STATE_FINISH;

  *status = SL_STATUS_IS_WAITING;
}

static void file_update_case_recovery(sl_status_t *status)
{
  /*
   * FUTURO:
   *
   * Aqui sera implementado o mecanismo de recuperacao.
   *
   * A versao na NOR ainda representa a ultima atualizacao que terminou com sucesso,
   * portando ela pode ser usada futuramente para localizar e restaurar o backup correspondente.
   */

  printf("Recuperacao por backup ainda nao implementada.\r\n");

  state = FILE_UPDATE_STATE_FINISH;
  *status = SL_STATUS_IS_WAITING;
}

static void file_update_case_finish(sl_status_t *status)
{
  sl_zigbee_event_set_inactive(&progress_event);
  sl_zigbee_event_set_inactive(&version_wait_event);

  file_list_clear();

  version_node = NULL;
  version_file = NULL;

  installed_version_data_size = 0U;
  version_read_done = false;
  version_wait_done = false;

  update_files_quantity = 0U;

  finished = false;

  state = FILE_UPDATE_STATE_INIT;

  *status = SL_STATUS_OK;
}

static void file_update_case_error(sl_status_t *status)
{
  printf("Erro durante atualizacao.\r\n");

  sl_zigbee_event_set_inactive(&progress_event);
  sl_zigbee_event_set_inactive(&version_wait_event);

  /*
   * IMPORTANTE:
   *
   * A nova versao ainda NAO foi gravada na NOR.
   *
   * Portanto a NOR continua contendo a ultima versao que teve sucesso.
   *
   * Futuramente o estado RECOVERY podera usar essa informacao para restaurar os arquivos de backup.
   */
  state = FILE_UPDATE_STATE_RECOVERY;

  *status = SL_STATUS_FAIL;
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

  version_node = NULL;
  version_file = NULL;

  update_files_quantity = 0U;
}

/*
 * Responsável por registrar todos os arquivos disponíveis para a atualização
 */
sl_status_t file_list_build(void)
{
  sl_status_t status;

//  status = file_list_add(get_file_5());
//
//  if(status != SL_STATUS_OK)
//    {
//      return status;
//    }

  status = file_list_add(get_file_13TouchFile());

  if(status != SL_STATUS_OK)
    {
      return status;
    }

//  status = file_list_add(get_file_14ShowFile());
//
//  if(status != SL_STATUS_OK)
//    {
//      return status;
//    }

//  status = file_list_add(get_file_22Config());
//
//  if(status != SL_STATUS_OK)
//    {
//      return status;
//    }

  status = file_list_add(get_file_gs_version());

  if(status != SL_STATUS_OK)
    {
      return status;
    }

//  status = file_list_add(get_file_60());
//
//  if(status != SL_STATUS_OK)
//    {
//      return status;
//    }
//
//  status = file_list_add(get_file_62());
//
//  if(status != SL_STATUS_OK)
//    {
//      return status;
//    }
  return status;
}

static file_node_t *find_version_node(void)
{
  file_node_t *node;

  node = file_list;

  while(node != NULL)
    {
      if(node->file == get_file_gs_version())
        {
          return node;
        }

      node = node->next;
    }

  return NULL;
}

static file_node_t *find_next_update_file(file_node_t *node)
{
  while(node != NULL)
    {
      if(node != version_node)
        {
          return node;
        }

      node = node->next;
    }

  return NULL;
}

static uint16_t count_update_files(void)
{
  file_node_t *node;
  uint16_t count = 0U;

  node = file_list;

  while(node != NULL)
    {
      if(node != version_node)
        {
          count++;
        }

      node = node->next;
    }

  return count;
}

static void version_read_callback(sl_status_t status, uint16_t vp, const uint8_t *data, size_t data_size, void *context)
{
  (void)vp;
  (void)context;

  version_read_status = status;

  if(status == SL_STATUS_OK)
    {
      if((data == NULL) || (data_size < GS_VERSION_SIZE_BYTES))
        {
          version_read_status = SL_STATUS_INVALID_PARAMETER;
        }
      else
        {
          memcpy(installed_version_data, data, GS_VERSION_SIZE_BYTES);

          installed_version_data_size = GS_VERSION_SIZE_BYTES;
        }
    }

  version_read_done = true;
}
