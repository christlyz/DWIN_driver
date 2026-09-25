/*******************************************************************************
 * File Name.h
 *
 * Created on: May 27, 2026
 * Author Christian dos Santos
 *
 ******************************************************************************/

/*******************************************************************************
 * Description
 *
 * Usage:
 * Known Errors:
 * ToDo:
 ******************************************************************************/

/*******************************************************************************
 * Multiple include protection
 ******************************************************************************/

#ifndef DWIN_UPDATE_H_
#define DWIN_UPDATE_H_

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <stdint.h>
#include <stdbool.h>

#include "sl_status.h"

#include "dwin_file.h"
//#include "../DWIN_functions/dwin_service.h"
//#include "../DWIN_functions/dwin_widget.h"
/*******************************************************************************
 * Macros
 ******************************************************************************/

/*******************************************************************************
 * Defines
 ******************************************************************************/

/*******************************************************************************
 * Typedef & Enums
 ******************************************************************************/
typedef enum
{
  DWIN_UPDATE_STATE_ENABLE_CRC,
  DWIN_UPDATE_STATE_WAIT_CRC_ENABLE,
  DWIN_UPDATE_STATE_LOAD_BLOCK,
  DWIN_UPDATE_STATE_WRITE_RAM,
  DWIN_UPDATE_STATE_WAIT_RAM_ACK,
  DWIN_UPDATE_STATE_FLASH_WRITE,
  DWIN_UPDATE_STATE_WAIT_FLASH,
  DWIN_UPDATE_STATE_NEXT_BLOCK,
  DWIN_UPDATE_STATE_DISABLE_CRC,
  DWIN_UPDATE_STATE_WAIT_CRC_DISABLE,
  DWIN_UPDATE_STATE_ERROR_WAIT_CRC_DISABLE,
  DWIN_UPDATE_STATE_FINISH,
  DWIN_UPDATE_STATE_ERROR
} dwin_update_state_t;

typedef enum
{
  DWIN_UPDATE_ERROR_NONE,
  DWIN_UPDATE_ERROR_FILE_OPEN,
  DWIN_UPDATE_ERROR_FILE_READ,
  DWIN_UPDATE_ERROR_INVALID_FILE,
  DWIN_UPDATE_ERROR_UNSUPPORTED_FILE,
  DWIN_UPDATE_ERROR_FILE_SIZE,
  DWIN_UPDATE_ERROR_FILE_EMPTY,
  DWIN_UPDATE_ERROR_DWIN_WRITE,
  DWIN_UPDATE_ERROR_DWIN_STATUS,
  DWIN_UPDATE_ERROR_FLASH_WRITE,
  DWIN_UPDATE_ERROR_TIMEOUT,
  DWIN_UPDATE_ERROR_INVALID_STATE,
  DWIN_UPDATE_ERROR_RETRY
} dwin_update_error_t;

/*******************************************************************************
 * Interface Funtions
 ******************************************************************************/
sl_status_t dwin_update_open_file(const char *filename);

/*
 * Responsável por iniciar a atualização de um arquivo.
 */
sl_status_t dwin_update_start(dwin_update_file_t *file, bool *finish);

/*
 * Responsável por verificar se existe uma atualização em andamento.
 */
bool dwin_update_is_active(void);

/*
 * Responsável por retornar o estado atual da atualização.
 */
dwin_update_state_t dwin_update_get_state(void);

/*
 * Responsável por retornar o último erro ocorrido na atualização.
 */
dwin_update_error_t dwin_update_get_error(void);

/*
 * Responsável por retornar o progresso atual da atualização.
 */
uint8_t dwin_update_get_progress(void);
/*******************************************************************************
 * End
 ******************************************************************************/
#endif
