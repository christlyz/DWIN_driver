/*
 * dwin_update_test.c
 *
 *  Created on: 29 de set. de 2026
 *      Author: christian.santos
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dwin_update_test.h"
#include "dwin_update.h"
#include "file.h"

#define DWIN_UPDATE_TEST_DUMMY_DATA_SIZE 2U

/******************************************************************************/
/** Private typedefs                                                         **/
/******************************************************************************/

typedef enum
{
  DWIN_UPDATE_TEST_STATE_IDLE = 0U,
  DWIN_UPDATE_TEST_STATE_START,
  DWIN_UPDATE_TEST_STATE_WAIT_UPDATE,
  DWIN_UPDATE_TEST_STATE_WAIT_MANUAL,
  DWIN_UPDATE_TEST_STATE_WAIT_RETRY,
  DWIN_UPDATE_TEST_STATE_NEXT
} dwin_update_test_state_t;

/******************************************************************************/
/** Private variables                                                        **/
/******************************************************************************/
extern file_node_t *file_list;
static file_node_t *test_current_file = NULL;

static dwin_update_file_t *test_file = NULL;
static dwin_update_test_result_t test_results[DWIN_UPDATE_TEST_COUNT];
static dwin_update_test_id_t test_current = DWIN_UPDATE_TEST_VALID_FILE;
static dwin_update_test_state_t test_state = DWIN_UPDATE_TEST_STATE_IDLE;
static sl_status_t test_last_status = SL_STATUS_OK;

static bool test_active = false;
static bool test_full_sequence = false;
static bool test_finished = false;
static bool test_retry_started = false;

/******************************************************************************/
/** Private test objects                                                     **/
/******************************************************************************/

/*
 * Dados mínimos utilizados para testes de validação dos parâmetros.
 */
static const uint8_t test_dummy_data[DWIN_UPDATE_TEST_DUMMY_DATA_SIZE] =
    {
        0x00,
        0x00
    };

/*
 * Arquivo vazio.
 */
static dwin_update_file_t test_empty_file =
    {
        .name = "32.icl",
        .data = test_dummy_data,
        .size = 0U,
        .position = 0U
    };

/*
 * Arquivo com extensão inválida.
 */
static dwin_update_file_t test_incompatible_file =
    {
        .name = "32.txt",
        .data = test_dummy_data,
        .size = sizeof(test_dummy_data),
        .position = 0U
    };

/******************************************************************************/
/** Private function prototypes                                              **/
/******************************************************************************/
static bool dwin_update_test_valid_id(dwin_update_test_id_t test_id);
static void dwin_update_test_reset_results(void);
static void dwin_update_test_set_result(dwin_update_test_id_t test_id, dwin_update_test_result_t result);
static void dwin_update_test_start_current(void);
static void dwin_update_test_wait_update(void);
static void dwin_update_test_finish_current(void);
static void dwin_update_test_prepare_manual(const char *message);
static const char *dwin_update_test_get_name(dwin_update_test_id_t test_id);
static const char *dwin_update_test_get_result_name(dwin_update_test_result_t result);

/******************************************************************************/
/** Public functions                                                         **/
/******************************************************************************/
/*
 * Inicializa o módulo de testes.
 */
void dwin_update_test_init(void)
{
  file_list_clear();
  dwin_update_test_reset_results();

  test_file = NULL;
  test_current_file = NULL;

  test_current = DWIN_UPDATE_TEST_VALID_FILE;
  test_state = DWIN_UPDATE_TEST_STATE_IDLE;

  test_last_status = SL_STATUS_OK;

  test_active = false;
  test_full_sequence = false;
  test_finished = false;
  test_retry_started = false;
}

/*
 * Define o arquivo utilizado pelos testes que trabalham com um único arquivo.
 */
sl_status_t dwin_update_test_set_file(dwin_update_file_t *file)
{
  if(file == NULL)
    {
      return SL_STATUS_INVALID_PARAMETER;
    }

  test_file = file;

  return SL_STATUS_OK;
}

/*
 * Inicia um teste específico.
 */
sl_status_t dwin_update_test_start(dwin_update_test_id_t test_id)
{
  if(!dwin_update_test_valid_id(test_id))
    {
      return SL_STATUS_INVALID_PARAMETER;
    }

  if(test_active)
    {
      return SL_STATUS_BUSY;
    }

  test_current = test_id;

  test_state = DWIN_UPDATE_TEST_STATE_START;

  test_active = true;
  test_full_sequence = false;
  test_finished = false;
  test_retry_started = false;

  test_last_status = SL_STATUS_OK;

  dwin_update_test_set_result(test_id, DWIN_UPDATE_TEST_RESULT_RUNNING);

  return SL_STATUS_OK;
}


/*
 * Inicia todos os testes em sequência.
 */
sl_status_t dwin_update_test_start_all(void)
{
  if(test_active)
    {
      return SL_STATUS_BUSY;
    }

  test_current = DWIN_UPDATE_TEST_VALID_FILE;

  test_state = DWIN_UPDATE_TEST_STATE_START;

  test_active = true;
  test_full_sequence = true;
  test_finished = false;
  test_retry_started = false;

  test_last_status = SL_STATUS_OK;

  dwin_update_test_reset_results();

  dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_RUNNING);

  return SL_STATUS_OK;
}

/*
 * Processa a máquina de estados dos testes.
 */
void dwin_update_test_process(void)
{
  if(!test_active)
    {
      return;
    }

  switch(test_state)
  {
    case DWIN_UPDATE_TEST_STATE_START:
      dwin_update_test_start_current();
      break;

    case DWIN_UPDATE_TEST_STATE_WAIT_UPDATE:
      dwin_update_test_wait_update();
      break;

    case DWIN_UPDATE_TEST_STATE_WAIT_MANUAL:
      // Aguarda dwin_update_test_confirm().
      break;

    case DWIN_UPDATE_TEST_STATE_WAIT_RETRY:
      // Aguarda dwin_update_test_retry().
      break;

    case DWIN_UPDATE_TEST_STATE_NEXT:
      test_state = DWIN_UPDATE_TEST_STATE_START;
      break;

    case DWIN_UPDATE_TEST_STATE_IDLE:
    default:
      test_active = false;
      test_state = DWIN_UPDATE_TEST_STATE_IDLE;
      break;
  }
}

/*
 * Confirma manualmente o resultado de um teste que depende de observação física.
 */
sl_status_t dwin_update_test_confirm(bool pass)
{
  if(!test_active ||
      test_state != DWIN_UPDATE_TEST_STATE_WAIT_MANUAL)
    {
      return SL_STATUS_INVALID_STATE;
    }

  dwin_update_test_set_result(test_current,
                              pass ? DWIN_UPDATE_TEST_RESULT_PASS
                                  : DWIN_UPDATE_TEST_RESULT_FAIL);

  printf("Teste %02u - %s: %s\r\n",
         (unsigned)(test_current + 1U),
         dwin_update_test_get_name(test_current),
         pass ? "PASS" : "FAIL");

  if(test_full_sequence)
    {
      test_state = DWIN_UPDATE_TEST_STATE_NEXT;
    }
  else
    {
      test_active = false;
      test_state = DWIN_UPDATE_TEST_STATE_IDLE;
    }

  return SL_STATUS_OK;
}

/*
 * Solicita a segunda tentativa do teste de retry.
 */
sl_status_t dwin_update_test_retry(void)
{
  if(!test_active ||
      test_current != DWIN_UPDATE_TEST_RETRY_AFTER_FAILURE ||
      test_state != DWIN_UPDATE_TEST_STATE_WAIT_RETRY)
    {
      return SL_STATUS_INVALID_STATE;
    }

  if(test_file == NULL)
    {
      return SL_STATUS_INVALID_STATE;
    }

  test_file->position = 0U;
  test_finished = false;
  test_retry_started = true;

  test_last_status = dwin_update_start(test_file, &test_finished);

  if(test_last_status != SL_STATUS_OK)
    {
      dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_FAIL);

      test_active = false;
      test_state = DWIN_UPDATE_TEST_STATE_IDLE;

      return test_last_status;
    }

  test_state = DWIN_UPDATE_TEST_STATE_WAIT_UPDATE;

  printf("Teste %02u - %s: segunda tentativa iniciada.\r\n",
         (unsigned)(test_current + 1U),
         dwin_update_test_get_name(test_current));

  return SL_STATUS_OK;
}

/*
 * Retorna o resultado de um teste.
 */
dwin_update_test_result_t dwin_update_test_get_result(dwin_update_test_id_t test_id)
{
  if(!dwin_update_test_valid_id(test_id))
    {
      return DWIN_UPDATE_TEST_RESULT_NOT_RUN;
    }

  return test_results[test_id];
}

/*
 * Retorna o teste atualmente executado.
 */
dwin_update_test_id_t dwin_update_test_get_current(void)
{
  return test_current;
}

/*
 * Retorna o resultado do teste atual.
 */
dwin_update_test_result_t dwin_update_test_get_current_result(void)
{
  return dwin_update_test_get_result(test_current);
}

/*
 * Retorna o último status recebido.
 */
sl_status_t dwin_update_test_get_last_status(void)
{
  return test_last_status;
}

/*
 * Retorna o progresso atual da atualização.
 */
uint8_t dwin_update_test_get_progress(void)
{
  return dwin_update_get_progress();
}

/*
 * Retorna true enquanto a máquina de testes estiver executando.
 */
bool dwin_update_test_is_active(void)
{
  return test_active;
}

/*
 * Imprime o resumo dos testes.
 */
void dwin_update_test_print_summary(void)
{
  uint8_t index;

  printf("\r\n");
  printf("==================================================\r\n");
  printf("DWIN UPDATE TEST SUMMARY\r\n");
  printf("==================================================\r\n");

  for(index = 0U; index < DWIN_UPDATE_TEST_COUNT; index++)
    {
      printf("%02u - %-35s : %s\r\n",
             (unsigned)(index + 1U),
             dwin_update_test_get_name((dwin_update_test_id_t)index),
             dwin_update_test_get_result_name(test_results[index]));
    }
  printf("==================================================\r\n");
}

/******************************************************************************/
/** Private functions                                                        **/
/******************************************************************************/
static void dwin_update_test_start_current(void)
{
  switch(test_current)
  {
    case DWIN_UPDATE_TEST_EMPTY_FILE:
      test_empty_file.position = 0U;
      test_last_status = dwin_update_start(&test_empty_file, &test_finished);

      if(test_last_status == SL_STATUS_INVALID_PARAMETER)
        {
          dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_PASS);
        }
      else
        {
          dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_FAIL);
        }

      test_state = DWIN_UPDATE_TEST_STATE_NEXT;
      break;

    case DWIN_UPDATE_TEST_INCOMPATIBLE_FILE:
      test_incompatible_file.position = 0U;

      test_last_status = dwin_update_start(&test_incompatible_file, &test_finished);

      if(test_last_status == SL_STATUS_INVALID_PARAMETER)
        {
          dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_PASS);
        }
      else
        {
          dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_FAIL);
        }

      test_state = DWIN_UPDATE_TEST_STATE_NEXT;
      break;

    case DWIN_UPDATE_TEST_CORRUPTED_FILE:
      /*
       * Atualmente dwin_update_start() valida estrutura, tamanho, extensão e ID,
       * mas não possui verificação de integridade do conteúdo do arquivo.
       */
      dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_NOT_SUPPORTED);

      printf("Teste %02u - %s: verificacao de integridade ainda nao esta disponivel.\r\n",
             (unsigned)(test_current + 1U),
             dwin_update_test_get_name(test_current));

      test_state = DWIN_UPDATE_TEST_STATE_NEXT;
      break;

    case DWIN_UPDATE_TEST_CANCEL:
      /*
       * Ainda não existe uma API pública dwin_update_cancel()
       */
      dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_NOT_SUPPORTED);

      printf("Teste %02u - %s: cancelamento ainda nao esta disponivel.\r\n",
             (unsigned)(test_current + 1U),
             dwin_update_test_get_name(test_current));

      test_state = DWIN_UPDATE_TEST_STATE_NEXT;

      break;

    case DWIN_UPDATE_TEST_POWER_LOSS:
      dwin_update_test_prepare_manual("Desligue a alimentacao durante a atualizacao e verifique o comportamento apos religar.");
      break;

    case DWIN_UPDATE_TEST_VALID_FILE:
    case DWIN_UPDATE_TEST_COMPLETE_UPDATE:
    case DWIN_UPDATE_TEST_DWIN_INIT:
      if(test_file == NULL)
        {
          test_last_status = SL_STATUS_INVALID_STATE;

          dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_FAIL);

          test_state = DWIN_UPDATE_TEST_STATE_NEXT;
          break;
        }

      test_file->position = 0U;
      test_finished = false;
      test_last_status = dwin_update_start(test_file, &test_finished);

      if(test_last_status != SL_STATUS_OK)
        {
          dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_FAIL);

          test_state = DWIN_UPDATE_TEST_STATE_NEXT;
          break;
        }

      printf("Teste %02u - %s iniciado.\r\n",
             (unsigned)(test_current + 1U),
             dwin_update_test_get_name(test_current));

      test_state = DWIN_UPDATE_TEST_STATE_WAIT_UPDATE;
      break;

    case DWIN_UPDATE_TEST_TRANSFER_TIMEOUT:
    case DWIN_UPDATE_TEST_COMMUNICATION_LOSS:
    case DWIN_UPDATE_TEST_START_FAILURE:
    case DWIN_UPDATE_TEST_MIDDLE_FAILURE:
    case DWIN_UPDATE_TEST_END_FAILURE:
      if(test_file == NULL)
        {
          test_last_status = SL_STATUS_INVALID_STATE;

          dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_FAIL);

          test_state = DWIN_UPDATE_TEST_STATE_NEXT;

          break;
        }

      test_file->position = 0U;
      test_finished = false;

      test_last_status = dwin_update_start(test_file, &test_finished);

      if(test_last_status != SL_STATUS_OK)
        {
          dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_FAIL);

          test_state = DWIN_UPDATE_TEST_STATE_NEXT;

          break;
        }

      printf("Teste %02u - %s iniciado.\r\n",
             (unsigned)(test_current + 1U),
             dwin_update_test_get_name(test_current));

      printf("Aplique a falha física correspondente ao teste.\r\n");

      test_state = DWIN_UPDATE_TEST_STATE_WAIT_UPDATE;
      break;

    case DWIN_UPDATE_TEST_RETRY_AFTER_FAILURE:
      if(test_file == NULL)
        {
          test_last_status = SL_STATUS_INVALID_STATE;
          dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_FAIL);

          test_state = DWIN_UPDATE_TEST_STATE_NEXT;
          break;
        }

      test_file->position = 0U;
      test_finished = false;
      test_retry_started = false;

      test_last_status = dwin_update_start(test_file, &test_finished);

      if(test_last_status != SL_STATUS_OK)
        {
          dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_FAIL);

          test_state = DWIN_UPDATE_TEST_STATE_NEXT;
          break;
        }

      printf("Teste %02u - %s: primeira tentativa iniciada.\r\n",
             (unsigned)(test_current + 1U),
             dwin_update_test_get_name(test_current));

      printf("Aplique uma falha e aguarde o estado ERROR.\r\n");
      test_state = DWIN_UPDATE_TEST_STATE_WAIT_UPDATE;

      break;

    case DWIN_UPDATE_TEST_CONSECUTIVE_UPDATES:
      if(file_list == NULL)
        {
          test_last_status = SL_STATUS_INVALID_STATE;

          dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_FAIL);

          test_state = DWIN_UPDATE_TEST_STATE_NEXT;

          break;
        }

      test_current_file = file_list;
      test_current_file->file->position = 0U;
      test_finished = false;

      test_last_status = dwin_update_start(test_current_file->file, &test_finished);

      if(test_last_status != SL_STATUS_OK)
        {
          dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_FAIL);

          test_state = DWIN_UPDATE_TEST_STATE_NEXT;

          break;
        }

      printf("Teste %02u - %s: primeiro arquivo iniciado.\r\n",
             (unsigned)(test_current + 1U),
             dwin_update_test_get_name(test_current));

      test_state = DWIN_UPDATE_TEST_STATE_WAIT_UPDATE;

      break;

    default:
      dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_FAIL);

      test_state = DWIN_UPDATE_TEST_STATE_NEXT;
      break;
  }
}

static void dwin_update_test_wait_update(void)
{
  dwin_update_state_t update_state;

  /*
   * O driver permanece ativo enquanto a FSM de atualização estiver trabalhando.
   */
  if(dwin_update_is_active())
    {
      return;
    }

  update_state = dwin_update_get_state();

  /*
   * Teste específico de retry.
   */
  if(test_current == DWIN_UPDATE_TEST_RETRY_AFTER_FAILURE && !test_retry_started)
    {
      if(update_state == DWIN_UPDATE_STATE_ERROR)
        {
          dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_WAITING);

          printf("Teste %02u - %s: falha detectada. Execute dwin_update_test_retry().\r\n", (unsigned)(test_current + 1U),
                 dwin_update_test_get_name(test_current));

          test_state = DWIN_UPDATE_TEST_STATE_WAIT_RETRY;

          return;
        }

      dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_FAIL);

      test_state = DWIN_UPDATE_TEST_STATE_NEXT;

      return;
    }

  /*
   * Se terminou em ERROR, os testes de falha precisam de confirmação manual.
   */
  if(update_state == DWIN_UPDATE_STATE_ERROR)
    {
      switch(test_current)
      {
        case DWIN_UPDATE_TEST_TRANSFER_TIMEOUT:
        case DWIN_UPDATE_TEST_COMMUNICATION_LOSS:
        case DWIN_UPDATE_TEST_START_FAILURE:
        case DWIN_UPDATE_TEST_MIDDLE_FAILURE:
        case DWIN_UPDATE_TEST_END_FAILURE:

          dwin_update_test_prepare_manual("Verifique se a falha foi detectada, se os retries foram executados e se o CRC foi tratado corretamente.");
          return;
        default:
          dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_FAIL);

          test_state = DWIN_UPDATE_TEST_STATE_NEXT;

          return;
      }
    }

  /*
   * Atualização finalizada.
   */
  if(test_finished)
    {
      if(dwin_update_get_progress() != 100U)
        {
          dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_FAIL);

          printf("Teste %02u - %s: terminou com progresso diferente de 100%%.\r\n",
                 (unsigned)(test_current + 1U), dwin_update_test_get_name(test_current));

          test_state = DWIN_UPDATE_TEST_STATE_NEXT;

          return;
        }

      /*
       * O teste de inicialização precisa observar a DWIN fisicamente.
       */
      if(test_current == DWIN_UPDATE_TEST_DWIN_INIT)
        {
          dwin_update_test_prepare_manual("Reinicie a DWIN e verifique se a tela inicializa corretamente.");
          return;
        }

      /*
       * Atualizações consecutivas:
       * não reseta a DWIN entre os arquivos.
       */
      if(test_current == DWIN_UPDATE_TEST_CONSECUTIVE_UPDATES)
        {
          if(test_current_file != NULL &&
              test_current_file->next != NULL)
            {
              test_current_file = test_current_file->next;

              test_current_file->file->position = 0U;
              test_finished = false;

              test_last_status = dwin_update_start(test_current_file->file, &test_finished);

              if(test_last_status != SL_STATUS_OK)
                {
                  dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_FAIL);

                  test_state = DWIN_UPDATE_TEST_STATE_NEXT;

                  return;
                }

              printf("Teste %02u - %s: proximo arquivo iniciado.\r\n",
                     (unsigned)(test_current + 1U),
                     dwin_update_test_get_name(test_current));
              return;
            }

          /*
           * Todos os arquivos foram concluídos.
           * O reset deve ser feito pelo gerenciador externo.
           */
          dwin_update_test_prepare_manual("Todos os arquivos foram atualizados. Verifique que existe apenas um reset da DWIN apos o ultimo arquivo.");
          return;
        }

      dwin_update_test_finish_current();

      return;
    }

  /*
   * Estado inesperado.
   */
  dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_FAIL);

  test_state = DWIN_UPDATE_TEST_STATE_NEXT;
}

static void dwin_update_test_finish_current(void)
{
  dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_PASS);

  printf("Teste %02u - %s: PASS\r\n",
         (unsigned)(test_current + 1U), dwin_update_test_get_name(test_current));

  if(test_full_sequence)
    {
      test_state = DWIN_UPDATE_TEST_STATE_NEXT;
    }
  else
    {
      test_active = false;
      test_active = DWIN_UPDATE_TEST_STATE_IDLE;
    }
}

static void dwin_update_test_prepare_manual(const char *message)
{
  dwin_update_test_set_result(test_current, DWIN_UPDATE_TEST_RESULT_WAITING);

  if(message != NULL)
    {
      printf("Teste %02u - %s: %s\r\n",
             (unsigned)(test_current + 1U), dwin_update_test_get_name(test_current), message);
    }

  test_state = DWIN_UPDATE_TEST_STATE_WAIT_MANUAL;
}

static bool dwin_update_test_valid_id(dwin_update_test_id_t test_id)
{
  return ((uint32_t)test_id < (uint32_t)DWIN_UPDATE_TEST_COUNT);
}

static void dwin_update_test_reset_results(void)
{
  uint8_t index;

  for(index = 0U; index < DWIN_UPDATE_TEST_COUNT; index++)
    {
      test_results[index] = DWIN_UPDATE_TEST_RESULT_NOT_RUN;
    }
}

static void dwin_update_test_set_result(dwin_update_test_id_t test_id, dwin_update_test_result_t result)
{
  if(dwin_update_test_valid_id(test_id))
    {
      test_results[test_id] = result;
    }
}

static const char *dwin_update_test_get_name(
    dwin_update_test_id_t test_id)
{
  static const char *const names[DWIN_UPDATE_TEST_COUNT] =
  {
    "Arquivo valido",
    "Arquivo vazio",
    "Arquivo corrompido",
    "Arquivo incompativel",
    "Atualizacao completa",
    "Timeout durante transferencia",
    "Perda de comunicacao",
    "Falha no inicio da atualizacao",
    "Falha no meio da atualizacao",
    "Falha proximo ao final",
    "Cancelamento durante atualizacao",
    "Nova tentativa apos falha",
    "Interrupcao de energia",
    "Inicializacao da DWIN",
    "Atualizacoes consecutivas"
  };

  if(!dwin_update_test_valid_id(test_id))
    {
      return "Teste invalido";
    }

  return names[test_id];
}

static const char *dwin_update_test_get_result_name(
    dwin_update_test_result_t result)
{
  switch(result)
  {
    case DWIN_UPDATE_TEST_RESULT_RUNNING:
      return "RUNNING";

    case DWIN_UPDATE_TEST_RESULT_WAITING:
      return "WAITING";

    case DWIN_UPDATE_TEST_RESULT_PASS:
      return "PASS";

    case DWIN_UPDATE_TEST_RESULT_FAIL:
      return "FAIL";

    case DWIN_UPDATE_TEST_RESULT_NOT_SUPPORTED:
      return "NOT_SUPPORTED";

    case DWIN_UPDATE_TEST_RESULT_NOT_RUN:
    default:
      return "NOT_RUN";
  }
}
