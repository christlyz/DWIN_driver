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
/*******************************************************************************
 * Data types
 ******************************************************************************/
static files current_file = DWIN_14SHOWFILE;
static dwin_update_file_t *file = NULL;
static bool finished = false;
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
    case DWIN_32:
      file = get_file_32();
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
