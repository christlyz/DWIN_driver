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
#include "dwin_file.h"
/*******************************************************************************
 * Macros
 ******************************************************************************/

/*******************************************************************************
 * Defines
 ******************************************************************************/
#define DWIN_UPDATE_BUFFER_SIZE 480U
#define DWIN_UPDATE_PACKET_SIZE 240U
#define DWIN_UPDATE_FLASH_BLOCK_SIZE_0X06 (28U * 1024)

#define DWIN_UPDATE_RAM_START         0x8000U

#define DWIN_UPDATE_FILL_VALUE  0x00U

#define DWIN_UPDATE_MAX_RETRIES 3U

#define DWIN_UPDATE_FLASH_BLOCK_SIZE_0XAA (32U * 1024)
#define DWIN_TEST_RAM_SIZE (32U * 1024U)
#define DWIN_TEST_FLASH_SIZE (32 * 1024U)
/*******************************************************************************
 * Typedef & Enums
 ******************************************************************************/
typedef enum
{
  DWIN_UPDATE_STATE_IDLE,
  DWIN_UPDATE_STATE_LOAD_BLOCK,
  DWIN_UPDATE_STATE_WRITE_RAM,
  DWIN_UPDATE_STATE_FILL_BLOCK,
  DWIN_UPDATE_STATE_FLASH_WRITE,
  DWIN_UPDATE_STATE_WAIT_FLASH,
  DWIN_UPDATE_STATE_VERIFY_BLOCK,
  DWIN_UPDATE_STATE_NEXT_BLOCK,
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
  DWIN_UPDATE_ERROR_RETRY
} dwin_update_error_t;

typedef enum
{
  DWIN_UPDATE_METHOD_INVALID,
  DWIN_UPDATE_METHOD_0xAA,
  DWIN_UPDATE_METHOD_0x06
} dwin_update_method_t;

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
  dwin_update_file_t *file;

  uint8_t id;
  dwin_update_extension_t extension;
  dwin_update_method_t method;

  uint32_t file_size;
  uint32_t file_offset;

  uint32_t file_id_base_block;
  uint32_t total_blocks;
  uint32_t current_block;

  uint32_t block_offset;
  uint32_t current_block_size;

  uint16_t ram_address;

  uint8_t buffer[DWIN_UPDATE_BUFFER_SIZE];
  size_t buffer_size;

  bool flash_status_pending;

  uint8_t retry_count;

  dwin_update_state_t state;
  dwin_update_error_t error;

  bool active;
} dwin_update_t;

/*******************************************************************************
 * Interface Funtions
 ******************************************************************************/
sl_status_t dwin_update_open_file(const char *filename);

sl_status_t dwin_update_start(dwin_update_file_t *file);

void dwin_update_process(void);

bool dwin_update_is_active(void);

dwin_update_state_t dwin_update_get_state(void);

dwin_update_error_t dwin_update_get_error(void);
/*******************************************************************************
 * End
 ******************************************************************************/
#endif
