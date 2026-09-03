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

#ifndef DWIN_SERVICE_H_
#define DWIN_SERVICE_H_

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "dwin_driver.h"
#include "dwin_widget.h"
#include <stdint.h>
#include <stdbool.h>
#include "string.h"

#include "zigbee_app_framework_event.h"
/*******************************************************************************
 * Macros
 ******************************************************************************/

/*******************************************************************************
 * Defines
 ******************************************************************************/
#define DWIN_STANDBY_BIT_CONTROL      0x04
#define DWIN_TOUCH_SOUND_BIT_CONTROL  0x08

#define DWIN_DEFAULT_BRIGHTNESS               100U
#define DWIN_DEFAULT_STANDBY_BRIGHTNESS       0U
#define DWIN_DEFAULT_STANDBY_TIMEOUT          10000
#define DWIN_DEFAULT_STANDBY_ACTIVATED        0
#define DWIN_DEFAULT_TOUCH_SOUND_ACTIVATED    0

#define DWIN_PAGE_ENABLE  0x5A
#define DWIN_PAGE_SWITCH  0x01

#define DWIN_WRITE_OK_1 0x4F
#define DWIN_WRITE_OK_2 0x4B

#define DWIN_HEADER_SIZE 3U
/*******************************************************************************
 * Typedef & Enums
 ******************************************************************************/
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

void dwin_poll();

sl_status_t dwin_configure_device();

sl_status_t dwin_set_icon(uint16_t vp, uint16_t icon);

sl_status_t dwin_write_text(uint16_t vp, uint8_t max_text_size, const char *text);
sl_status_t dwin_read_text(uint16_t vp, uint8_t expected_size, dwin_read_callback_t callback);
size_t dwin_extract_text(const uint8_t *data, size_t data_size, char *text, size_t text_size);
sl_status_t dwin_clear_text(uint16_t vp, uint8_t text_size);

sl_status_t dwin_write(uint16_t vp, size_t data_size, uint8_t *data);
sl_status_t dwin_read(uint16_t vp, size_t data_size, dwin_read_callback_t callback);

sl_status_t dwin_register_callback(uint16_t vp, uint8_t instruction, bool expect_data, uint16_t expected_data, dwin_vp_callback_t callback);
sl_status_t dwin_unregister_callback(uint16_t vp, uint8_t instruction, uint16_t expected_data);

sl_status_t dwin_read_vp_async(uint16_t vp, uint8_t words, uint32_t timeout_ms, dwin_read_callback_t callback);
sl_status_t dwin_cancel_read_vp(uint16_t vp);

sl_status_t dwin_change_page(uint16_t page);
sl_status_t dwin_play_buzzer_ms(uint16_t milisseconds);
/*******************************************************************************
 * End
 ******************************************************************************/
#endif
