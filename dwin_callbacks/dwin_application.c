/*******************************************************************************
 * File Name.c
 *
 * Created on: May 27, 2026
 * Author Christian dos Santos
 *
 ******************************************************************************/

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "dwin_application.h"
/*******************************************************************************
 * Data types
 ******************************************************************************/
uint16_t page;
sl_zigbee_event_t change_page_callback;
void change_page_handler(sl_zigbee_event_t *event);
dwin_config_t *my_dwin;
/*******************************************************************************
 * Extern
 ******************************************************************************/

/*******************************************************************************
 * Private Function Prototypes
 ******************************************************************************/

static void fire_button_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context);
static void fault_button_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context);
static void ok_button_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context);
static void return_button_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context);
static void input_text_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context);
/*******************************************************************************
 * Function name:
 *
 * Description:
 * Parameteres:
 * Returns:
 *
 * Known issues:
 * Note:
 ******************************************************************************/
void application_init()
{
  my_dwin = dwin_get_config();

  my_dwin->standby_brightness_activated = true;
  my_dwin->touch_sound_activated = false;
  my_dwin->brightness = 100;
  my_dwin->standby_brightness = 25;
  my_dwin->standby_timeout = 5000;

  if(dwin_configure_device() != SL_STATUS_OK)
    printf("Erro de configuracao");

  dwin_register_callback(
      DWIN_VP_ACTION_BUTTON,
      DWIN_CMD_READ,
      true,
      DWIN_BUTTON_FIRE,
      fire_button_callback);

  dwin_register_callback(
      DWIN_VP_ACTION_BUTTON,
      DWIN_CMD_READ,
      true,
      DWIN_BUTTON_FAULT,
      fault_button_callback);

  dwin_register_callback(
      DWIN_VP_ACTION_BUTTON,
      DWIN_CMD_READ,
      true,
      DWIN_BUTTON_DISABLE,
      ok_button_callback);

  dwin_register_callback(
      DWIN_VP_RETURN_BUTTON,
      DWIN_CMD_READ,
      false,
      2,
      return_button_callback);

  dwin_register_callback(
      DWIN_VP_FIRST_TEXT,
      DWIN_CMD_READ,
      false,
      0,
      input_text_callback);

  dwin_register_callback(
      DWIN_VP_SECOND_TEXT,
      DWIN_CMD_READ,
      false,
      0,
      input_text_callback);

  dwin_register_callback(
      DWIN_VP_THIRD_TEXT,
      DWIN_CMD_READ,
      false,
      0,
      input_text_callback);

//  dwin_play_buzzer_ms(250);
}

static void fire_button_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context)
{
  dwin_set_icon(DWIN_VP_ICON, DWIN_ICON_FIRE);
  dwin_write_text(DWIN_VP_TEXT_STATUS, DWIN_TEXT_SIZE, "FOGO");

  my_dwin->brightness = 50;
  dwin_configure_device();
}

static void fault_button_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context)
{
  dwin_set_icon(DWIN_VP_ICON, DWIN_ICON_FAULT);
  dwin_write_text(DWIN_VP_TEXT_STATUS, DWIN_TEXT_SIZE, "FALHA");

  my_dwin->brightness = 0;
  dwin_configure_device();
}

static void ok_button_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context)
{
  dwin_set_icon(DWIN_VP_ICON, DWIN_ICON_NORMAL);
  dwin_write_text(DWIN_VP_TEXT_STATUS, DWIN_TEXT_SIZE, "ESTADO NORMAL");

  my_dwin->brightness = 100;
  dwin_configure_device();

  dwin_change_page(DWIN_PAGE_TEXT);
}

static void return_button_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context)
{
  dwin_change_page(DWIN_PAGE_HOME);
}

static void input_text_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context)
{
  char text[32];

  printf("VP: 0x%04X\r\n", vp);
  dwin_extract_text(data, data_size, text, sizeof(text));
  printf("%s\r\n", text);
}

void change_page_handler(sl_zigbee_event_t *event)
{
  dwin_change_page(DWIN_PAGE_HOME);
}
