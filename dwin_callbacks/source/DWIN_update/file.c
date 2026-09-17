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
//    case DWIN_13TOUCHFILE:
//      file = get_file_13();
//      current_file = DWIN_14SHOWFILE;
//      status = SL_STATUS_IS_WAITING;
//      break;
    case DWIN_14SHOWFILE:
      file = get_file_14();
      status = dwin_update_start(file);
      if(status == SL_STATUS_OK)
        {
          current_file = DWIN_22_CONFIG;
          status = SL_STATUS_IS_WAITING;
        }
      break;
    case DWIN_22_CONFIG:
      file = get_file_22();
      status = dwin_update_start(file);
      if(status == SL_STATUS_OK)
        {
          current_file = DWIN_59;
          status = SL_STATUS_IS_WAITING;
        }
      break;
    case DWIN_59:
      file = get_file_59();
      status = dwin_update_start(file);
      if(status == SL_STATUS_OK)
        {
          current_file = DWIN_63;
          status = SL_STATUS_IS_WAITING;
        }
      break;
//    case DWIN_60:
//      file = get_file_60();
//      current_file = DWIN_62;
//      status = SL_STATUS_IS_WAITING;
//      break;
//    case DWIN_62:
//      file = get_file_62();
//      status = dwin_update_start(file);
//      if(status == SL_STATUS_OK)
//        {
//          current_file = DWIN_63;
//          status = SL_STATUS_IS_WAITING;
//        }
//      break;
    case DWIN_63:
      file = get_file_63();
      status = dwin_update_start(file);
      if(status == SL_STATUS_OK)
        {
          current_file = DWIN_FINISH;
          file = NULL;
        }
      break;
    default:
      break;
  }
  return status;
}
