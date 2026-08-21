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

#ifndef DWIN_DRIVER_H_
#define DWIN_DRIVER_H_

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "sl_iostream.h"
#include "em_usart.h"
#include "usart.h"

#include "display.h"
#include "zigbee_app_framework_event.h"
/*******************************************************************************
 * Macros
 ******************************************************************************/

/*******************************************************************************
 * Defines
 ******************************************************************************/
#define DWIN_HEADER_1 0x5A
#define DWIN_HEADER_2 0xA5
#define DWIN_HEADER_SIZE 3U

#define DWIN_CMD_WRITE 0x82
#define DWIN_CMD_READ 0x83

#define DWIN_WRITE_OK_1 0x4F
#define DWIN_WRITE_OK_2 0x4B

#define DWIN_MAX_PACKET_SIZE 255U
#define DWIN_MAX_DATA_LENGTH 249U

#define DWIN_PAGE_ENABLE  0x5A
#define DWIN_PAGE_SWITCH  0x01

#define DWIN_BRIGHTNESS_CMD 0x0A

#define DWIN_VP_BRIGHTNESS  0x0082
#define DWIN_VP_PAGE        0x0084
/*******************************************************************************
 * Typedef & Enums
 ******************************************************************************/
typedef struct
{
  uint8_t instruction;
  uint16_t vp;
  uint8_t words;
  const uint8_t *data;
} dwin_packet_t;

typedef void (*dwin_vp_callback_t) (uint16_t vp, const uint8_t *data, size_t data_size, void *context);

typedef void (*dwin_read_callback_t) (sl_status_t status, uint16_t vp, const uint8_t *data, size_t data_size, void *context);
/*******************************************************************************
 * Interface Funtions
 ******************************************************************************/
sl_status_t dwin_register_callback(uint16_t vp, uint8_t instruction, uint16_t expected_data, dwin_vp_callback_t callback);
sl_status_t dwin_unregister_callback(uint16_t vp, uint8_t instruction, uint16_t expected_data);

sl_status_t dwin_read_vp_async(uint16_t vp, uint8_t words, uint32_t timeout_ms, dwin_read_callback_t callback);
sl_status_t dwin_cancel_read_vp(uint16_t vp);

void dwin_poll();


sl_status_t dwin_write_vp(uint16_t vp, const uint8_t *data, size_t size);
sl_status_t dwin_read_vp(uint16_t vp, uint8_t words);
sl_status_t dwin_change_page(uint16_t page);
sl_status_t dwin_change_brightness(uint8_t brightness);
/*******************************************************************************
 * End
 ******************************************************************************/
#endif
