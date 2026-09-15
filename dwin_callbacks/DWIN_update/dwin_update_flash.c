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
#include "dwin_update_flash.h"
/*******************************************************************************
 * Data types
 ******************************************************************************/

/*******************************************************************************
 * Extern
 ******************************************************************************/
extern dwin_update_t update;
/*******************************************************************************
 * Private Function Prototypes
 ******************************************************************************/
static void flash_status_callback(sl_status_t status,
                                  uint16_t vp,
                                  const uint8_t *data,
                                  size_t data_size,
                                  void *context);
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
sl_status_t dwin_update_flash_write_block(uint16_t flash_block, uint16_t ram_address, uint16_t delay_ms)
{
  uint8_t data[12];

  data[0] = 0x5A;
  data[1] = 0x02;

  data[2] = (uint8_t)(flash_block >> 8);
  data[3] = (uint8_t)(flash_block & 0xFFU);

  data[4] = (uint8_t)(ram_address >> 8);
  data[5] = (uint8_t)(ram_address & 0xFFU);

  data[6] = (uint8_t)(delay_ms >> 8);
  data[7] = (uint8_t)(delay_ms & 0xFFU);

  data[8] = 0x00;
  data[9] = 0x00;
  data[10] = 0x00;
  data[11] = 0x00;

  return dwin_write(
      DWIN_UPDATE_VP_EXTERNAL_FLASH,
      sizeof(data),
      data);
}

sl_status_t dwin_update_flash_request_status(void)
{
  if(update.method == DWIN_UPDATE_METHOD_0xAA)
    {
//      return dwin_read_vp_async(DWIN_UPDATE_VP_EXTERNAL_FLASH,
//                                1U,
//                                DWIN_UPDATE_FLASH_STATUS_TIMEOUT_MS,
//                                flash_status_callback);
      flash_status_callback(SL_STATUS_OK,
                            DWIN_UPDATE_VP_EXTERNAL_FLASH,
                            (const uint8_t[]){0x00, 0x02},
                            2U,
                            NULL);
      return SL_STATUS_OK;
    }
  else
    {
      /*
       * Ainda nao implementado o metodo 0x06
       */
    }
  return SL_STATUS_NOT_FOUND;
}

static void flash_status_callback(sl_status_t status,
                                  uint16_t vp,
                                  const uint8_t *data,
                                  size_t data_size,
                                  void *context)
{
  (void) vp;
  (void) context;

  update.flash_status_pending = false;
  if(status != SL_STATUS_OK)
    {
      update.error = DWIN_UPDATE_ERROR_DWIN_STATUS;
      update.state = DWIN_UPDATE_STATE_ERROR;
      return;
    }

  if(data == NULL || data_size < 2U)
    {
      update.error = DWIN_UPDATE_ERROR_DWIN_STATUS;
      update.state = DWIN_UPDATE_STATE_ERROR;
      return;
    }

  if(data[0] == 0x00 &&
      data[1] == 0x02)
    {
      update.state = DWIN_UPDATE_STATE_NEXT_BLOCK;
      return;
    }

  if(data[0] == 0x5A &&
      data[1] == 0x02)
    {
      update.state = DWIN_UPDATE_STATE_WAIT_FLASH;
      return;
    }

  update.error = DWIN_UPDATE_ERROR_DWIN_STATUS;
  update.state = DWIN_UPDATE_STATE_ERROR;
}
