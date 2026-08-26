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

#define DWIN_VP_PAGE        0x0084
#define DWIN_VP_BRIGHTNESS  0x0082

#define DWIN_VP_BUZZER      0x00A0

/**
 * .7: Serial port CRC check   0=off  1=on read only
 * .6: Reserved, write 0
 * .5: Load 22 file initialization VP at power-on   1=Load    0=unload  read only
 * .4: Automatic upload setting  1=On  0=Off    read and write
 * .3: Touch panel audio control  1=Open  0=Close   read and write
 * .2: Touch panel standby backlight standby control    1=On  0=Off   read and write
 * .1-0: Display direction 00=0°  01=90°  10=180° 11=270°   read and write
 **/
#define DWIN_VP_SYSTEM_CONFIG 0x0080

#define DWIN_STANDBY_BIT_CONTROL      0x04
#define DWIN_TOUCH_SOUND_BIT_CONTROL  0x08

#define DWIN_DEFAULT_BRIGHTNESS               100U
#define DWIN_DEFAULT_STANDBY_BRIGHTNESS       0U
#define DWIN_DEFAULT_STANDBY_TIMEOUT          10000
#define DWIN_DEFAULT_STANDBY_ACTIVATED        0
#define DWIN_DEFAULT_TOUCH_SOUND_ACTIVATED    0
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

typedef struct
{
  bool standby_brightness_activated;
  bool touch_sound_activated;
  uint8_t brightness;
  uint8_t standby_brightness;
  uint16_t standby_timeout;
} dwin_config_t;

typedef void (*dwin_vp_callback_t) (uint16_t vp, const uint8_t *data, size_t data_size, void *context);

typedef void (*dwin_read_callback_t) (sl_status_t status, uint16_t vp, const uint8_t *data, size_t data_size, void *context);
/*******************************************************************************
 * Interface Funtions
 ******************************************************************************/
dwin_config_t* dwin_get_config();
sl_status_t dwin_register_callback(uint16_t vp, uint8_t instruction, uint16_t expected_data, dwin_vp_callback_t callback);
sl_status_t dwin_unregister_callback(uint16_t vp, uint8_t instruction, uint16_t expected_data);

sl_status_t dwin_read_vp_async(uint16_t vp, uint8_t words, uint32_t timeout_ms, dwin_read_callback_t callback);
sl_status_t dwin_cancel_read_vp(uint16_t vp);

void dwin_poll();

sl_status_t dwin_write_vp(uint16_t vp, const uint8_t *data, size_t size);
sl_status_t dwin_read_vp(uint16_t vp, uint8_t words);

sl_status_t dwin_change_page(uint16_t page);

sl_status_t dwin_configure_device();

sl_status_t dwin_play_buzzer_ms(uint16_t milisseconds);
/*******************************************************************************
 * End
 ******************************************************************************/
#endif
