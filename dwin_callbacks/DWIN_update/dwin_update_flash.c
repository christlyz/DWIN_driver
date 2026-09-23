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
static void dwin_update_flash_status_callback(sl_status_t status,
                                  uint16_t vp,
                                  const uint8_t *data,
                                  size_t data_size,
                                  void *context);
static void dwin_update_flash_write_ack_callback(sl_status_t status, uint16_t vp, void *context);
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
sl_status_t dwin_update_flash_write_block()
{
  uint8_t data[12];

  data[0] = 0x5A;
  data[1] = 0x02;

  data[2] = (uint8_t)(update.current_block >> 8);
  data[3] = (uint8_t)(update.current_block & 0xFFU);

  data[4] = (uint8_t)(update.ram_address >> 8);
  data[5] = (uint8_t)(update.ram_address & 0xFFU);

  data[6] = 0x00U;
  data[7] = 0x14U;

  data[8] = 0x00U;
  data[9] = 0x00U;
  data[10] = 0x00U;
  data[11] = 0x00U;

  return dwin_write_vp_async(DWIN_UPDATE_VP_EXTERNAL_FLASH,
                             data,
                             sizeof(data),
                             DWIN_UPDATE_FLASH_WRITE_TIMEOUT_MS,
                             dwin_update_flash_write_ack_callback,
                             NULL);
}

static void dwin_update_flash_write_ack_callback(sl_status_t status, uint16_t vp, void *context)
{
  (void)vp;
  (void)context;

  if(status != SL_STATUS_OK)
    {
      if(dwin_update_is_recoverable_error(status) &&
          update.flash_retry_count < DWIN_UPDATE_MAX_RETRIES)
        {
          update.flash_retry_count++;

          printf("Falha gravacao Flash - Retry %u/%u\r\n", update.flash_retry_count, DWIN_UPDATE_MAX_RETRIES);

          update.state = DWIN_UPDATE_STATE_FLASH_WRITE;
          return;
        }

      update.error = DWIN_UPDATE_ERROR_DWIN_WRITE;
      update.state = DWIN_UPDATE_STATE_ERROR;
      return;
    }

  update.flash_retry_count = 0U;
  update.flash_status_retry_count = 0U;
  update.flash_status_pending = false;
  update.state = DWIN_UPDATE_STATE_WAIT_FLASH;
}
sl_status_t dwin_update_flash_request_status(void)
{
  return dwin_read_vp_async(DWIN_UPDATE_VP_EXTERNAL_FLASH,
                            1U,
                            DWIN_UPDATE_FLASH_STATUS_TIMEOUT_MS,
                            dwin_update_flash_status_callback);
}

static void dwin_update_flash_status_callback(sl_status_t status,
                                  uint16_t vp,
                                  const uint8_t *data,
                                  size_t data_size,
                                  void *context)
{
  (void)vp;
  (void)context;

  update.flash_status_pending = false;

  if(status != SL_STATUS_OK)
    {
      if(dwin_update_is_recoverable_error(status) &&
          update.flash_status_retry_count < DWIN_UPDATE_MAX_RETRIES)
        {
          update.flash_status_retry_count++;

          printf("Falha consulta status Flash - retry %u/%u\r\n",
                 update.flash_status_retry_count,
                 DWIN_UPDATE_MAX_RETRIES);

          update.state = DWIN_UPDATE_STATE_WAIT_FLASH;
          return;
        }
      update.error = DWIN_UPDATE_ERROR_DWIN_STATUS;
      update.state = DWIN_UPDATE_STATE_ERROR;
      return;
    }

  if(data == NULL ||
      data_size < 2U)
    {
      update.error = DWIN_UPDATE_ERROR_DWIN_STATUS;
      update.state = DWIN_UPDATE_STATE_ERROR;
      return;
    }

  if(data[0] == 0x5AU && data[1] == 0x02U)
    {
      /*
       * Flash ainda está gravando.
       */
      update.flash_status_retry_count = 0U;
      update.state = DWIN_UPDATE_STATE_WAIT_FLASH;
      return;
    }

  if(data[0] == 0x00U && data[1] == 0x02U)
    {
      /*
       * Flash terminou.
       */
      update.flash_status_retry_count = 0U;
      update.state = DWIN_UPDATE_STATE_NEXT_BLOCK;
      update.progress = dwin_update_get_progress();
      return;
    }

  update.error = DWIN_UPDATE_ERROR_DWIN_STATUS;
  update.state = DWIN_UPDATE_STATE_ERROR;
}
