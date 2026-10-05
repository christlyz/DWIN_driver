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
static void dwin_update_test_next(void);
static void dwin_update_test_finish_current(void);
static const char *dwin_update_test_get_name(dwin_update_test_id_t test_id);
static const char *dwin_update_test_get_result_name(dwin_update_test_result_t result);
static void dwin_update_test_complete_current(void);

static bool dwin_update_test_is_fault_test(dwin_update_test_id_t test_id);
static dwin_update_inject_fault_t dwin_update_test_get_fault(dwin_update_test_id_t test_id);
/******************************************************************************/
/** Public functions                                                         **/
/******************************************************************************/
/*
 * Inicializa o módulo de testes.
 */
void dwin_update_test_init(void)
{
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

  dwin_update_clear_injected_fault();
}

/*
 * Define o arquivo usado nos testes.
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
 * Inicia um teste.
 */
sl_status_t dwin_update_test_start(dwin_update_test_id_t test_id)
{
  if(!dwin_update_test_valid_id(test_id) || test_active)
    {
      return test_active ? SL_STATUS_BUSY : SL_STATUS_INVALID_PARAMETER;
    }

  test_current = test_id;
  test_state = DWIN_UPDATE_TEST_STATE_START;
  test_active = true;
  test_full_sequence = false;
  test_finished = false;
  test_retry_started = false;
  test_current_file = NULL;
  test_last_status = SL_STATUS_OK;

  dwin_update_clear_injected_fault();

  dwin_update_test_set_result(
      test_current,
      DWIN_UPDATE_TEST_RESULT_RUNNING);

  return SL_STATUS_OK;
}

/*
 * Inicia todos os testes.
 */
sl_status_t dwin_update_test_start_all(void)
{
  if(test_active)
    {
      return SL_STATUS_BUSY;
    }

  dwin_update_test_reset_results();

  test_current = DWIN_UPDATE_TEST_VALID_FILE;
  test_state = DWIN_UPDATE_TEST_STATE_START;
  test_active = true;
  test_full_sequence = true;
  test_finished = false;
  test_retry_started = false;
  test_current_file = NULL;
  test_last_status = SL_STATUS_OK;

  dwin_update_clear_injected_fault();

  dwin_update_test_set_result(
      test_current,
      DWIN_UPDATE_TEST_RESULT_RUNNING);

  return SL_STATUS_OK;
}

/*
 * Processa a máquina de estados.
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

    case DWIN_UPDATE_TEST_STATE_NEXT:
      dwin_update_test_next();
      break;

    case DWIN_UPDATE_TEST_STATE_IDLE:
    default:
      test_active = false;
      test_state = DWIN_UPDATE_TEST_STATE_IDLE;
      break;
  }
}

/*
 * Retorna resultado.
 */
dwin_update_test_result_t dwin_update_test_get_result(
    dwin_update_test_id_t test_id)
{
  if(!dwin_update_test_valid_id(test_id))
    {
      return DWIN_UPDATE_TEST_RESULT_NOT_RUN;
    }

  return test_results[test_id];
}

/*
 * Retorna teste atual.
 */
dwin_update_test_id_t dwin_update_test_get_current(void)
{
  return test_current;
}

/*
 * Retorna resultado do teste atual.
 */
dwin_update_test_result_t dwin_update_test_get_current_result(void)
{
  return dwin_update_test_get_result(test_current);
}

/*
 * Retorna último status.
 */
sl_status_t dwin_update_test_get_last_status(void)
{
  return test_last_status;
}

/*
 * Retorna progresso.
 */
uint8_t dwin_update_test_get_progress(void)
{
  return dwin_update_get_progress();
}

/*
 * Informa se os testes estão ativos.
 */
bool dwin_update_test_is_active(void)
{
  return test_active;
}

/*
 * Imprime resumo.
 */
void dwin_update_test_print_summary(void)
{
  uint8_t i;

  printf("\r\n");
  printf("==================================================\r\n");
  printf("DWIN UPDATE TEST SUMMARY\r\n");
  printf("==================================================\r\n");

  for(i = 0U; i < DWIN_UPDATE_TEST_COUNT; i++)
  {
    printf("%02u - %-35s : %s\r\n",
           (unsigned)(i + 1U),
           dwin_update_test_get_name(
               (dwin_update_test_id_t)i),
           dwin_update_test_get_result_name(
               test_results[i]));
  }

  printf("==================================================\r\n");
}

/******************************************************************************/
/** Private functions                                                        **/
/******************************************************************************/

/*
 * Inicia o arquivo especificado e entra em WAIT_UPDATE.
 */
static bool dwin_update_test_start_file(
    dwin_update_file_t *file)
{
  if(file == NULL)
    {
      test_last_status = SL_STATUS_INVALID_STATE;
      return false;
    }

  file->position = 0U;
  test_finished = false;

  dwin_update_clear_injected_fault();

  test_last_status =
      dwin_update_start(file, &test_finished);

  if(test_last_status != SL_STATUS_OK)
    {
      return false;
    }

  test_state = DWIN_UPDATE_TEST_STATE_WAIT_UPDATE;
  return true;
}

/*
 * Finaliza o teste atual.
 */
static void dwin_update_test_finish_current(void)
{
  dwin_update_test_set_result(
      test_current,
      DWIN_UPDATE_TEST_RESULT_PASS);

  printf("Teste %02u - %s: PASS\r\n",
         (unsigned)(test_current + 1U),
         dwin_update_test_get_name(test_current));

  dwin_update_test_complete_current();
}

/*
 * Inicia o teste atual.
 */
static void dwin_update_test_start_current(void)
{
  dwin_update_inject_fault_t fault;

  switch(test_current)
  {
    case DWIN_UPDATE_TEST_EMPTY_FILE:
      test_empty_file.position = 0U;

      test_last_status =
          dwin_update_start(
              &test_empty_file,
              &test_finished);

      dwin_update_test_set_result(
          test_current,
          test_last_status == SL_STATUS_INVALID_PARAMETER
          ? DWIN_UPDATE_TEST_RESULT_PASS
          : DWIN_UPDATE_TEST_RESULT_FAIL);

      dwin_update_test_complete_current();
      return;

    case DWIN_UPDATE_TEST_INCOMPATIBLE_FILE:
      test_incompatible_file.position = 0U;

      test_last_status =
          dwin_update_start(
              &test_incompatible_file,
              &test_finished);

      dwin_update_test_set_result(
          test_current,
          test_last_status == SL_STATUS_INVALID_PARAMETER
          ? DWIN_UPDATE_TEST_RESULT_PASS
          : DWIN_UPDATE_TEST_RESULT_FAIL);

      dwin_update_test_complete_current();
      return;

    case DWIN_UPDATE_TEST_CORRUPTED_FILE:
    case DWIN_UPDATE_TEST_CANCEL:
    case DWIN_UPDATE_TEST_POWER_LOSS:

      dwin_update_test_set_result(
          test_current,
          DWIN_UPDATE_TEST_RESULT_NOT_SUPPORTED);

      printf("Teste %02u - %s: NOT_SUPPORTED\r\n",
             (unsigned)(test_current + 1U),
             dwin_update_test_get_name(test_current));

      dwin_update_test_complete_current();
      return;

    case DWIN_UPDATE_TEST_VALID_FILE:
    case DWIN_UPDATE_TEST_COMPLETE_UPDATE:
    case DWIN_UPDATE_TEST_DWIN_INIT:

      if(!dwin_update_test_start_file(test_file))
      {
        dwin_update_test_set_result(
            test_current,
            DWIN_UPDATE_TEST_RESULT_FAIL);

        dwin_update_test_complete_current();
        return;
      }

      printf("Teste %02u - %s iniciado.\r\n",
             (unsigned)(test_current + 1U),
             dwin_update_test_get_name(test_current));

      return;

    case DWIN_UPDATE_TEST_TRANSFER_TIMEOUT:
    case DWIN_UPDATE_TEST_COMMUNICATION_LOSS:
    case DWIN_UPDATE_TEST_START_FAILURE:
    case DWIN_UPDATE_TEST_MIDDLE_FAILURE:
    case DWIN_UPDATE_TEST_END_FAILURE:

      if(test_file == NULL)
      {
        test_last_status = SL_STATUS_INVALID_STATE;

        dwin_update_test_set_result(
            test_current,
            DWIN_UPDATE_TEST_RESULT_FAIL);

        dwin_update_test_complete_current();
        return;
      }

      test_file->position = 0U;
      test_finished = false;

      fault = dwin_update_test_get_fault(test_current);

      dwin_update_clear_injected_fault();
      dwin_update_inject_fault(fault);

      printf("Teste %02u - %s iniciado.\r\n",
             (unsigned)(test_current + 1U),
             dwin_update_test_get_name(test_current));

      /*
       * Falha de início acontece dentro de dwin_update_start().
       */
      if(test_current == DWIN_UPDATE_TEST_START_FAILURE)
      {
        test_last_status =
            dwin_update_start(
                test_file,
                &test_finished);

        if(test_last_status != SL_STATUS_OK &&
           dwin_update_fault_was_triggered())
        {
          dwin_update_test_set_result(
              test_current,
              DWIN_UPDATE_TEST_RESULT_PASS);
        }
        else
        {
          dwin_update_test_set_result(
              test_current,
              DWIN_UPDATE_TEST_RESULT_FAIL);
        }

        dwin_update_clear_injected_fault();
        dwin_update_test_complete_current();

        return;
      }

      /*
       * Demais falhas acontecem durante a transferência.
       */
      test_last_status =
          dwin_update_start(
              test_file,
              &test_finished);

      if(test_last_status != SL_STATUS_OK)
      {
        dwin_update_clear_injected_fault();

        dwin_update_test_set_result(
            test_current,
            DWIN_UPDATE_TEST_RESULT_FAIL);

        dwin_update_test_complete_current();
        return;
      }

      test_state =
          DWIN_UPDATE_TEST_STATE_WAIT_UPDATE;

      return;

    case DWIN_UPDATE_TEST_RETRY_AFTER_FAILURE:

      if(!dwin_update_test_start_file(test_file))
      {
        dwin_update_test_set_result(
            test_current,
            DWIN_UPDATE_TEST_RESULT_FAIL);

        dwin_update_test_complete_current();
        return;
      }

      /*
       * Primeira tentativa deve falhar no meio.
       */
      dwin_update_clear_injected_fault();

      dwin_update_inject_fault(
          DWIN_UPDATE_INJECT_MIDDLE_FAILURE);

      printf("Teste %02u - %s: primeira tentativa iniciada.\r\n",
             (unsigned)(test_current + 1U),
             dwin_update_test_get_name(test_current));

      return;

    case DWIN_UPDATE_TEST_CONSECUTIVE_UPDATES:

      if(file_list == NULL)
      {
        test_last_status = SL_STATUS_INVALID_STATE;

        dwin_update_test_set_result(
            test_current,
            DWIN_UPDATE_TEST_RESULT_FAIL);

        dwin_update_test_complete_current();
        return;
      }

      test_current_file = file_list;

      if(!dwin_update_test_start_file(
             test_current_file->file))
      {
        dwin_update_test_set_result(
            test_current,
            DWIN_UPDATE_TEST_RESULT_FAIL);

        dwin_update_test_complete_current();
        return;
      }

      printf("Teste %02u - %s: primeiro arquivo iniciado.\r\n",
             (unsigned)(test_current + 1U),
             dwin_update_test_get_name(test_current));

      return;

    default:

      dwin_update_test_set_result(
          test_current,
          DWIN_UPDATE_TEST_RESULT_FAIL);

      dwin_update_test_complete_current();
      return;
  }
}

/*
 * Aguarda a conclusão da atualização.
 */
static void dwin_update_test_wait_update(void)
{
  dwin_update_state_t state;

  if(dwin_update_is_active())
    {
      return;
    }

  state = dwin_update_get_state();

  /*
   * ----------------------------------------------------------
   * RETRY
   * ----------------------------------------------------------
   */
  if(test_current == DWIN_UPDATE_TEST_RETRY_AFTER_FAILURE)
  {
    if(!test_retry_started &&
       state == DWIN_UPDATE_STATE_ERROR)
    {
      printf("[RETRY] Primeira tentativa falhou.\r\n");

      dwin_update_clear_injected_fault();

      test_retry_started = true;
      test_file->position = 0U;
      test_finished = false;

      dwin_update_test_set_result(
          test_current,
          DWIN_UPDATE_TEST_RESULT_RUNNING);

      test_last_status =
          dwin_update_start(
              test_file,
              &test_finished);

      if(test_last_status != SL_STATUS_OK)
      {
        dwin_update_test_set_result(
            test_current,
            DWIN_UPDATE_TEST_RESULT_FAIL);

        dwin_update_test_complete_current();
        return;
      }

      printf("[RETRY] Segunda tentativa iniciada.\r\n");

      test_state =
          DWIN_UPDATE_TEST_STATE_WAIT_UPDATE;

      return;
    }

    if(test_retry_started &&
       state == DWIN_UPDATE_STATE_ERROR)
    {
      printf("[RETRY] Segunda tentativa falhou.\r\n");

      dwin_update_test_set_result(
          test_current,
          DWIN_UPDATE_TEST_RESULT_FAIL);

      dwin_update_test_complete_current();

      return;
    }

    if(test_retry_started && test_finished)
    {
      printf("[RETRY] Segunda tentativa concluida.\r\n");

      dwin_update_test_finish_current();
      return;
    }

    return;
  }

  /*
   * ----------------------------------------------------------
   * TESTES DE FALHA
   * ----------------------------------------------------------
   */
  if(dwin_update_test_is_fault_test(test_current))
  {
    if(state == DWIN_UPDATE_STATE_ERROR)
    {
      printf("[TEST] Falha detectada.\r\n");
      printf("Estado   : %u\r\n",
             (unsigned)state);
      printf("Status   : 0x%08lx\r\n",
             (unsigned long)dwin_update_get_last_status());
      printf("Progresso: %u%%\r\n",
             (unsigned)dwin_update_get_progress());

      dwin_update_test_set_result(
          test_current,
          dwin_update_fault_was_triggered()
          ? DWIN_UPDATE_TEST_RESULT_PASS
          : DWIN_UPDATE_TEST_RESULT_FAIL);

      dwin_update_clear_injected_fault();

      dwin_update_test_complete_current();
      return;
    }

    if(test_finished)
    {
      printf("[TEST] Falha esperada nao ocorreu.\r\n");

      dwin_update_test_set_result(
          test_current,
          DWIN_UPDATE_TEST_RESULT_FAIL);

      dwin_update_clear_injected_fault();

      dwin_update_test_complete_current();
      return;
    }

    return;
  }

  /*
   * ----------------------------------------------------------
   * ERRO INESPERADO
   * ----------------------------------------------------------
   */
  if(state == DWIN_UPDATE_STATE_ERROR)
  {
    printf("[TEST] Erro inesperado.\r\n");
    printf("Estado: %u\r\n",
           (unsigned)state);
    printf("Status: 0x%08lx\r\n",
           (unsigned long)dwin_update_get_last_status());

    dwin_update_test_set_result(
        test_current,
        DWIN_UPDATE_TEST_RESULT_FAIL);

    dwin_update_test_complete_current();
    return;
  }

  /*
   * ----------------------------------------------------------
   * ATUALIZAÇÃO CONCLUÍDA
   * ----------------------------------------------------------
   */
  if(test_finished)
  {
    /*
     * Atualizações consecutivas.
     */
    if(test_current == DWIN_UPDATE_TEST_CONSECUTIVE_UPDATES)
    {
      if(test_current_file != NULL &&
         test_current_file->next != NULL)
      {
        test_current_file =
            test_current_file->next;

        if(!dwin_update_test_start_file(
               test_current_file->file))
        {
          dwin_update_test_set_result(
              test_current,
              DWIN_UPDATE_TEST_RESULT_FAIL);

          dwin_update_test_complete_current();
          return;
        }

        printf("Proximo arquivo: %s\r\n",
               test_current_file->file->name);

        return;
      }

      printf("Todos os arquivos foram atualizados.\r\n");

      dwin_update_test_finish_current();
      return;
    }

    /*
     * Todos os outros testes concluídos.
     */
    dwin_update_test_finish_current();
    return;
  }

  /*
   * Caso inesperado.
   */
  printf("[TEST] Estado inesperado.\r\n");

  dwin_update_test_set_result(
      test_current,
      DWIN_UPDATE_TEST_RESULT_FAIL);

  dwin_update_test_complete_current();
}

/*
 * Avança para o próximo teste.
 */
static void dwin_update_test_next(void)
{
  if(!test_full_sequence)
  {
    test_active = false;
    test_state = DWIN_UPDATE_TEST_STATE_IDLE;
    return;
  }

  if((uint32_t)test_current + 1U >=
     (uint32_t)DWIN_UPDATE_TEST_COUNT)
  {
    test_active = false;
    test_state = DWIN_UPDATE_TEST_STATE_IDLE;

    printf("\r\nTodos os testes foram executados.\r\n");
    return;
  }

  test_current++;

  test_finished = false;
  test_retry_started = false;
  test_current_file = NULL;
  test_last_status = SL_STATUS_OK;

  dwin_update_clear_injected_fault();

  dwin_update_test_set_result(
      test_current,
      DWIN_UPDATE_TEST_RESULT_RUNNING);

  test_state = DWIN_UPDATE_TEST_STATE_START;
}

/*
 * Finaliza o teste atual e escolhe o próximo estado.
 */
static void dwin_update_test_complete_current(void)
{
  if(test_full_sequence)
  {
    test_state = DWIN_UPDATE_TEST_STATE_NEXT;
  }
  else
  {
    test_active = false;
    test_state = DWIN_UPDATE_TEST_STATE_IDLE;
  }
}

/*
 * Verifica se o teste é de falha.
 */
static bool dwin_update_test_is_fault_test(
    dwin_update_test_id_t test_id)
{
  return (test_id >= DWIN_UPDATE_TEST_TRANSFER_TIMEOUT &&
          test_id <= DWIN_UPDATE_TEST_END_FAILURE);
}

/*
 * Obtém o tipo de falha correspondente ao teste.
 */
static dwin_update_inject_fault_t
dwin_update_test_get_fault(dwin_update_test_id_t test_id)
{
  switch(test_id)
  {
    case DWIN_UPDATE_TEST_TRANSFER_TIMEOUT:
      return DWIN_UPDATE_INJECT_TIMEOUT_TRANSFER;

    case DWIN_UPDATE_TEST_COMMUNICATION_LOSS:
      return DWIN_UPDATE_INJECT_COMMUNICATION_LOSS;

    case DWIN_UPDATE_TEST_START_FAILURE:
      return DWIN_UPDATE_INJECT_START_FAILURE;

    case DWIN_UPDATE_TEST_MIDDLE_FAILURE:
      return DWIN_UPDATE_INJECT_MIDDLE_FAILURE;

    case DWIN_UPDATE_TEST_END_FAILURE:
      return DWIN_UPDATE_INJECT_END_FAILURE;

    default:
      return DWIN_UPDATE_INJECT_NONE;
  }
}

/*
 * Valida ID.
 */
static bool dwin_update_test_valid_id(
    dwin_update_test_id_t test_id)
{
  return ((uint32_t)test_id <
          (uint32_t)DWIN_UPDATE_TEST_COUNT);
}

/*
 * Reseta resultados.
 */
static void dwin_update_test_reset_results(void)
{
  uint8_t i;

  for(i = 0U; i < DWIN_UPDATE_TEST_COUNT; i++)
  {
    test_results[i] =
        DWIN_UPDATE_TEST_RESULT_NOT_RUN;
  }
}

/*
 * Define resultado.
 */
static void dwin_update_test_set_result(
    dwin_update_test_id_t test_id,
    dwin_update_test_result_t result)
{
  if(dwin_update_test_valid_id(test_id))
  {
    test_results[test_id] = result;
  }
}

/*
 * Nome dos testes.
 */
static const char *dwin_update_test_get_name(
    dwin_update_test_id_t test_id)
{
  static const char *const names[] =
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

/*
 * Nome dos resultados.
 */
static const char *dwin_update_test_get_result_name(
    dwin_update_test_result_t result)
{
  switch(result)
  {
    case DWIN_UPDATE_TEST_RESULT_RUNNING:
      return "RUNNING";

    case DWIN_UPDATE_TEST_RESULT_PASS:
      return "PASS";

    case DWIN_UPDATE_TEST_RESULT_FAIL:
      return "FAIL";

    case DWIN_UPDATE_TEST_RESULT_NOT_SUPPORTED:
      return "NOT_SUPPORTED";

    case DWIN_UPDATE_TEST_RESULT_WAITING:
      return "WAITING";

    case DWIN_UPDATE_TEST_RESULT_NOT_RUN:
    default:
      return "NOT_RUN";
  }
}
