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
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>

#include "sl_status.h"
/*******************************************************************************
 * Macros
 ******************************************************************************/

/*******************************************************************************
 * Defines
 ******************************************************************************/
#define DWIN_UPDATE_BUFFER_SIZE 480U
#define DWIN_UPDATE_PACKET_SIZE 240U
#define DWIN_UPDATE_FLASH_BLOCK_SIZE (32U * 1024)

#define DWIN_UPDATE_RAM_START         0x8000U
#define DWIN_UPDATE_VP_EXTERNAL_FLASH 0x00AAU

#define DWIN_UPDATE_MAX_RETRIES 3U
/*******************************************************************************
 * Typedef & Enums
 ******************************************************************************/
typedef enum
{
  DWIN_UPDATE_IDLE,
  DWIN_UPDATE_LOAD_BLOCK,
  DWIN_UPDATE_WRITE_RAM,
  DWIN_UPDATE_FLASH_WRITE,
  DWIN_UPDATE_WAIT_FLASH,
  DWIN_UPDATE_NEXT_BLOCK,
  DWIN_UPDATE_FINISH,
  DWIN_UPDATE_ERROR
} dwin_update_state_t;

typedef enum
{
  DWIN_UPDATE_ERROR_NONE,
  DWIN_UPDATE_ERROR_FILE_OPEN,
  DWIN_UPDATE_ERROR_FILE_READ,
  DWIN_UPDATE_ERROR_INVALID_FILE,
  DWIN_UPDATE_ERROR_UNSUPPORTED_FILE,
  DWIN_UPDATE_ERROR_FILE_SIZE,
  DWIN_UPDATE_ERROR_DWIN_WRITE,
  DWIN_UPDATE_ERROR_DWIN_STATUS,
  DWIN_UPDATE_ERROR_TIMEOUT,
  DWIN_UPDATE_ERROR_RETRY
} dwin_update_error_t;

typedef enum
{
  DWIN_UPDATE_EXTENSION_BIN,
  DWIN_UPDATE_EXTENSION_HZK,
  DWIN_UPDATE_EXTENSION_ICL,
  DWIN_UPDATE_EXTENSION_WAE,
  DWIN_UPDATE_EXTENSION_ERROR
} dwin_update_extension_t;

typedef struct
{
  FILE *file;

  uint8_t id;
  dwin_update_extension_t extension;

  uint32_t file_size;
  uint32_t file_offset;

  uint32_t total_blocks;
  uint32_t current_block;

  uint32_t block_offset;
  uint32_t current_block_size;

  uint16_t ram_address;

  uint8_t buffer[DWIN_UPDATE_BUFFER_SIZE];
  size_t buffer_size;

  uint8_t retry_count;

  dwin_update_state_t state;
  dwin_update_error_t error;

  bool active;
} dwin_update_t;

/*******************************************************************************
 * Interface Funtions
 ******************************************************************************/
sl_status_t dwin_update_open_file(const char *filename);

void dwin_update_process(void);

bool dwin_update_is_active(void);

dwin_update_state_t dwin_update_get_state(void);

dwin_update_error_t dwin_update_get_error(void);

void dwin_update_process(void);

bool dwin_update_is_active(void);
dwin_update_state_t dwin_update_get_state(void);
dwin_update_error_t dwin_update_get_error(void);
/*******************************************************************************
 * End
 ******************************************************************************/
#endif
