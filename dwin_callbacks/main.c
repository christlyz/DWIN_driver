/***************************************************************************//**
 * @file main.c
 * @brief main() function.
 *******************************************************************************
 * # License
 * <b>Copyright 2021 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#ifdef SL_COMPONENT_CATALOG_PRESENT
#include "sl_component_catalog.h"
#endif
#include "sl_system_init.h"
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif
#if defined(SL_CATALOG_KERNEL_PRESENT)
#include "sl_system_kernel.h"
#else
#include "sl_system_process_action.h"
#endif // SL_CATALOG_KERNEL_PRESENT

#ifdef EMBER_TEST
#define main nodeMain
#endif

#include "display.h"
#include "dwin_driver.h"

#include "zigbee_app_framework_event.h"

static void check_page_callback(sl_status_t status, uint16_t vp, const uint8_t *data, size_t data_size, void *context);

static void fire_button_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context);
static void fault_button_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context);
static void ok_button_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context);

uint16_t page;
sl_zigbee_event_t change_page_callback;
void change_page_handler(sl_zigbee_event_t *event);
uint8_t callbacks = 0;
dwin_config_t *my_dwin;

void app_init(void)
{
  my_dwin = dwin_get_config();

  my_dwin->standby_brightness_activated = true;
  my_dwin->touch_sound_activated = true;
  my_dwin->brightness = 100;
  my_dwin->standby_brightness = 25;
  my_dwin->standby_timeout = 5000;

  if(dwin_configure_device() != SL_STATUS_OK)
    printf("Erro de configuracao");

  if(dwin_register_callback(
      DWIN_VP_BUTTON,
      DWIN_CMD_READ,
      0,
      fire_button_callback) == SL_STATUS_OK)
    callbacks++;

  if(dwin_register_callback(
      DWIN_VP_BUTTON,
      DWIN_CMD_READ,
      1,
      fault_button_callback) == SL_STATUS_OK)
    callbacks++;

  if(dwin_register_callback(
      DWIN_VP_BUTTON,
      DWIN_CMD_READ,
      2,
      ok_button_callback) == SL_STATUS_OK)
    callbacks++;

  printf("Callbacks %d\r\n", callbacks);

  page = 0;
  dwin_change_page(page);

  dwin_play_buzzer_ms(250);
  sl_zigbee_event_init(&change_page_callback, change_page_handler);
}

void app_process_action(void)
{
  dwin_poll();
}

static void fire_button_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context)
{
  dwin_set_icon(DWIN_ICON_FIRE);
  dwin_write_text("FOGO");

  my_dwin->brightness = 50;
  dwin_configure_device();

  dwin_change_page(1);
  sl_zigbee_event_set_delay_ms(&change_page_callback, 5000);
}

static void fault_button_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context)
{
  dwin_set_icon(DWIN_ICON_FAULT);
  dwin_write_text("FALHA");

  my_dwin->brightness = 0;
  dwin_configure_device();
}

static void ok_button_callback(uint16_t vp, const uint8_t *data, size_t data_size, void *context)
{
  dwin_set_icon(DWIN_ICON_NORMAL);
  dwin_write_text("ESTADO NORMAL");

  my_dwin->brightness = 100;
  dwin_configure_device();

  dwin_read_vp_async(0x014,
                     1,
                     1000,
                     check_page_callback);
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
      printf("%02X", data[i]);
    }
  printf("\n");
}

void change_page_handler(sl_zigbee_event_t *event)
{
  dwin_change_page(0);
}

#if defined(__ICCARM__)
#pragma diag_suppress=Pe111
#endif // defined(__ICCARM__)
int main(void)
{
  // Initialize Silicon Labs device, system, service(s) and protocol stack(s).
  // Note that if the kernel is present, processing task(s) will be created by
  // this call.
  sl_system_init();

  // Initialize the application. For example, create periodic timer(s) or
  // task(s) if the kernel is present.
  app_init();

#if defined(SL_CATALOG_KERNEL_PRESENT)
  // Start the kernel. Task(s) created in app_init() will start running.
  sl_system_kernel_start();
#else // SL_CATALOG_KERNEL_PRESENT
  while (1) {
    // Do not remove this call: Silicon Labs components process action routine
    // must be called from the super loop.
    sl_system_process_action();

    // Application process.
    app_process_action();

    // Let the CPU go to sleep if the system allow it.
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
    sl_power_manager_sleep();
#endif // SL_CATALOG_POWER_MANAGER_PRESENT
  }
#endif // SL_CATALOG_KERNEL_PRESENT

  return 0;
}
