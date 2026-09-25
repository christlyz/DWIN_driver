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
#include "file.h"
#include "zigbee_app_framework_event.h"
#include <stdio.h>
/*******************************************************************************
 * Data types
 ******************************************************************************/
static files current_file = INIT_PROGRESS;
static dwin_update_file_t *file = NULL;
static bool finished = false;
static uint8_t progress = 0;

static sl_zigbee_event_t progress_event;
static void progress_handler(sl_zigbee_event_t *event);
/*******************************************************************************
 * Extern
 ******************************************************************************/

/*******************************************************************************
 * Private Function Prototypes
 ******************************************************************************/
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
sl_status_t start_update()
{
  sl_status_t status = SL_STATUS_OK;
  switch (current_file)
  {
    case INIT_PROGRESS:
      sl_zigbee_event_init(&progress_event, progress_handler);
      sl_zigbee_event_set_delay_ms(&progress_event, 1000);
      current_file = DWIN_32;
      status = SL_STATUS_IS_WAITING;
      break;
    case DWIN_32:
      printf("DWIN_32\r\n");
      file = get_file_32_two();
      status = dwin_update_start(file, &finished);
      if(status == SL_STATUS_OK)
        {
          current_file = DWIN_RESET;
          file = NULL;
          status = SL_STATUS_IS_WAITING;
        }
      break;
    case DWIN_RESET:
      if(finished)
        {
          sl_zigbee_event_set_inactive(&progress_event);
          dwin_reset();
          finished = false;
          return status;
        }
      status = SL_STATUS_IS_WAITING;
      break;
    default:
      break;
  }
  return status;
}

static void progress_handler(sl_zigbee_event_t *event)
{
  progress = dwin_update_get_progress();
  printf("Atualizacao %u%% concluida!\r\n", progress);

  sl_zigbee_event_set_delay_ms(&progress_event, 500);
}
