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
#include "display.h"

/*******************************************************************************
 * Data types
 ******************************************************************************/

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
sl_status_t dwin_set_icon(uint16_t icon)
{
  uint8_t data[2];
  data[0] = (uint8_t) (icon >> 8);
  data[1] = icon;

  sl_status_t status = dwin_write_vp(DWIN_VP_ICON, data, sizeof(data));
  return status;
}

sl_status_t dwin_write_text(const char *text)
{
  if(text == NULL)
    return SL_STATUS_NULL_POINTER;

  dwin_clear_text();

  size_t length = strlen(text);

  if(length > DWIN_TEXT_SIZE)
    length = DWIN_TEXT_SIZE;

  sl_status_t status = dwin_write_vp(DWIN_VP_TEXT, (const uint8_t*) text,length);
  return status;
}

sl_status_t dwin_clear_text()
{
  uint8_t buffer[DWIN_TEXT_SIZE];
  memset(buffer, ' ', sizeof(buffer));

  sl_status_t status = dwin_write_vp(DWIN_VP_TEXT, buffer, sizeof(buffer));
  return status;
}
