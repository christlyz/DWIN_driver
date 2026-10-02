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
#include "usart.h"
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
sl_status_t usart_write(uint8_t *tx, size_t length)
{
  if(tx == NULL || length == 0U)
    return SL_STATUS_NULL_POINTER;

  sl_status_t status = sl_iostream_write(sl_iostream_dwin_handle, tx, length);
  if(status != SL_STATUS_OK)
    {
      printf("Write: %lu\r\n", status);
    }
  return status;
}

sl_status_t usart_read(uint8_t *rx, size_t length, size_t *received)
{
  if(rx == NULL || received == NULL)
    return SL_STATUS_NULL_POINTER;

  *received = 0U;

  sl_status_t status =  sl_iostream_read(sl_iostream_dwin_handle, rx, length, received);
  return status;
}
