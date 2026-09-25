/*
 * dwin_update_internal.h
 *
 *  Created on: 25 de set. de 2026
 *      Author: christian.santos
 */

#ifndef DWIN_UPDATE_DWIN_UPDATE_INTERNAL_H_
#define DWIN_UPDATE_DWIN_UPDATE_INTERNAL_H_

#include "dwin_update.h"
#include "dwin_file.h"

#define DWIN_UPDATE_BUFFER_SIZE                  480U
#define DWIN_UPDATE_PACKET_SIZE                 240U

#define DWIN_UPDATE_FLASH_BLOCK_SIZE_0X06       (28U * 1024U)
#define DWIN_UPDATE_FLASH_BLOCK_SIZE_0XAA       (32U * 1024U)

#define DWIN_UPDATE_RAM_START                   0x8000U
#define DWIN_UPDATE_FILL_VALUE                  0x00U
#define DWIN_UPDATE_MAX_RETRIES                3U

#define DWIN_UPDATE_FLASH_WRITE_TIMEOUT_MS      1000U
#define DWIN_UPDATE_RAM_WRITE_TIMEOUT_MS        1000U

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
  uint32_t block_file_offset;

  size_t current_packet_size;
  size_t buffer_offset;

  uint16_t ram_address;

  uint8_t buffer[DWIN_UPDATE_BUFFER_SIZE];
  size_t buffer_size;

  bool flash_status_pending;

  uint8_t ram_retry_count;
  uint8_t flash_retry_count;
  uint8_t flash_status_retry_count;

  uint8_t progress;

  dwin_update_state_t state;
  dwin_update_error_t error;

  bool active;
} dwin_update_t;

extern dwin_update_t update;

bool dwin_update_is_recoverable_error(sl_status_t status);

void dwin_update_calculate_progress(void);
#endif /* DWIN_UPDATE_DWIN_UPDATE_INTERNAL_H_ */
