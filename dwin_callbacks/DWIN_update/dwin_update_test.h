/*
 * dwin_update_test.h
 *
 *  Created on: 29 de set. de 2026
 *      Author: christian.santos
 */

#ifndef DWIN_UPDATE_TEST_H_
#define DWIN_UPDATE_TEST_H_

#include <stdbool.h>
#include <stdint.h>

#include "sl_status.h"

#include "dwin_file.h"

typedef enum
{
  DWIN_UPDATE_TEST_VALID_FILE = 0U,
  DWIN_UPDATE_TEST_EMPTY_FILE,
  DWIN_UPDATE_TEST_CORRUPTED_FILE,
  DWIN_UPDATE_TEST_INCOMPATIBLE_FILE,
  DWIN_UPDATE_TEST_COMPLETE_UPDATE,
  DWIN_UPDATE_TEST_TRANSFER_TIMEOUT,
  DWIN_UPDATE_TEST_COMMUNICATION_LOSS,
  DWIN_UPDATE_TEST_START_FAILURE,
  DWIN_UPDATE_TEST_MIDDLE_FAILURE,
  DWIN_UPDATE_TEST_END_FAILURE,
  DWIN_UPDATE_TEST_CANCEL,
  DWIN_UPDATE_TEST_RETRY_AFTER_FAILURE,
  DWIN_UPDATE_TEST_POWER_LOSS,
  DWIN_UPDATE_TEST_DWIN_INIT,
  DWIN_UPDATE_TEST_CONSECUTIVE_UPDATES,

  DWIN_UPDATE_TEST_COUNT
} dwin_update_test_id_t;

typedef enum
{
  DWIN_UPDATE_TEST_RESULT_NOT_RUN = 0U,
  DWIN_UPDATE_TEST_RESULT_RUNNING,
  DWIN_UPDATE_TEST_RESULT_WAITING,
  DWIN_UPDATE_TEST_RESULT_PASS,
  DWIN_UPDATE_TEST_RESULT_FAIL,
  DWIN_UPDATE_TEST_RESULT_NOT_SUPPORTED
} dwin_update_test_result_t;

/*
 * Inicializa o módulo de testes.
 */
void dwin_update_test_init(void);

/*
 * Define o arquivo utilizado pelos testes que trabalham com um único arquivo.
 */
sl_status_t dwin_update_test_set_file(dwin_update_file_t *file);

/*
 * Inicia um teste específico.
 */
sl_status_t dwin_update_test_start(dwin_update_test_id_t test_id);

/*
 * Inicia todos os testes em sequência.
 */
sl_status_t dwin_update_test_start_all(void);

/*
 * Processa a máquina de estados dos testes.
 */
void dwin_update_test_process(void);

/*
 * Retorna o resultado de um teste.
 */
dwin_update_test_result_t dwin_update_test_get_result(dwin_update_test_id_t test_id);

/*
 * Retorna o teste atualmente executado.
 */
dwin_update_test_id_t dwin_update_test_get_current(void);

/*
 * Retorna o resultado do teste atual.
 */
dwin_update_test_result_t dwin_update_test_get_current_result(void);

/*
 * Retorna o último status recebido.
 */
sl_status_t dwin_update_test_get_last_status(void);

/*
 * Retorna o progresso atual da atualização.
 */
uint8_t dwin_update_test_get_progress(void);

/*
 * Retorna true enquanto a máquina de testes estiver executando.
 */
bool dwin_update_test_is_active(void);

/*
 * Imprime o resumo dos testes.
 */
void dwin_update_test_print_summary(void);

#endif /* DWIN_UPDATE_DWIN_UPDATE_TEST_H_ */
