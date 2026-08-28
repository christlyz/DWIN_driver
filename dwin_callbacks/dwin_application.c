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
sl_zigbee_event_t check_callback;
void check_page_handler(sl_zigbee_event_t *event);
uint8_t callbacks = 0;
dwin_config_t *my_dwin;
/*******************************************************************************
 * Extern
 ******************************************************************************/

/*******************************************************************************
 * Private Function Prototypes
 ******************************************************************************/
static void read_text_callback(sl_status_t status, uint16_t vp, const uint8_t *data, size_t data_size, void *context);
static void check_page_callback(sl_status_t status, uint16_t vp, const uint8_t *data, size_t data_size, void *context);

static void fire_button_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context);
static void fault_button_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context);
static void ok_button_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context);
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

  if(dwin_register_callback(
      DWIN_VP_BUTTON,
      DWIN_CMD_READ,
      DWIN_BUTTON_FIRE,
      fire_button_callback) == SL_STATUS_OK)
    callbacks++;

  if(dwin_register_callback(
      DWIN_VP_BUTTON,
      DWIN_CMD_READ,
      DWIN_BUTTON_FAULT,
      fault_button_callback) == SL_STATUS_OK)
    callbacks++;

  if(dwin_register_callback(
      DWIN_VP_BUTTON,
      DWIN_CMD_READ,
      DWIN_BUTTON_DISABLE,
      ok_button_callback) == SL_STATUS_OK)
    callbacks++;

  printf("Callbacks %d\r\n", callbacks);

  page = 0;
  if(dwin_change_page(page) == SL_STATUS_OK)
    printf("TROCOU PAGINA 0");
  else
    printf("NAO TROCOU PAGINA 0");

//  dwin_play_buzzer_ms(250);
  sl_zigbee_event_init(&change_page_callback, change_page_handler);
  sl_zigbee_event_init(&check_callback, check_page_handler);
  sl_zigbee_event_set_delay_ms(&check_callback, 1000);
}

static void fire_button_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context)
{
  dwin_set_icon(DWIN_VP_ICON, DWIN_ICON_FIRE);
  dwin_write_text(DWIN_VP_TEXT, DWIN_TEXT_SIZE, "FOGO");

  my_dwin->brightness = 50;
  dwin_configure_device();

  dwin_read_text(DWIN_VP_TEXT,
                 40,
                 read_text_callback);

}

static void fault_button_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context)
{
  dwin_set_icon(DWIN_VP_ICON, DWIN_ICON_FAULT);
  dwin_write_text(DWIN_VP_TEXT, DWIN_TEXT_SIZE, "FALHA");

  my_dwin->brightness = 0;
  dwin_configure_device();

  dwin_read_text(DWIN_VP_TEXT,
                 40,
                 read_text_callback);
}

static void ok_button_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context)
{
  dwin_set_icon(DWIN_VP_ICON, DWIN_ICON_NORMAL);
  dwin_write_text(DWIN_VP_TEXT, DWIN_TEXT_SIZE, "ESTADO NORMAL");

  my_dwin->brightness = 100;
  dwin_configure_device();

  dwin_change_page(1);
  sl_zigbee_event_set_delay_ms(&change_page_callback, 5000);

  dwin_read_text(DWIN_VP_TEXT,
                 20,
                 read_text_callback);

}

static void read_text_callback(sl_status_t status, uint16_t vp, const uint8_t *data, size_t data_size, void *context)
{
  if(status != SL_STATUS_OK)
    {
      printf("ERRO\r\n");
      return;
    }

//  char text[40];

//  dwin_extract_text(data, data_size, text, sizeof(text));

  printf("Texto lido: ");

  for(uint8_t i = 0; i < data_size; i++)
    {
      printf("%c", data);
    }
  printf("\r\n");
//  printf("%s\r\n", text);
}

void change_page_handler(sl_zigbee_event_t *event)
{
  dwin_change_page(DWIN_PAGE_HOME);
}

static void check_page_callback(sl_status_t status, uint16_t vp, const uint8_t *data, size_t data_size, void *context)
{
  if(status == SL_STATUS_TIMEOUT)
    {
      printf("ERRO\r\n");
      return;
    }

  printf("Pagina:\n");
  for(uint8_t i = 0; i < data_size; i++)
    {
      printf("%02X ", data[i]);
    }
  printf("\n");
}

void check_page_handler(sl_zigbee_event_t *event)
{
  dwin_read_vp_async(DWIN_VP_PAGE,
                     2,
                     1000,
                     check_page_callback);

  sl_zigbee_event_set_delay_ms(&check_callback, 1000);
}
