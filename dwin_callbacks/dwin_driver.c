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
#include "dwin_driver.h"
/*******************************************************************************
 * Data types
 ******************************************************************************/

/*******************************************************************************
 * Extern
 ******************************************************************************/

/*******************************************************************************
 * Private Function Prototypes
 ******************************************************************************/
static size_t build_write_packet(uint8_t *tx, uint16_t vp, const uint8_t *data, uint8_t data_size);
static size_t build_read_packet(uint8_t *tx, uint16_t vp, uint8_t words);
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
/*
 * Escreve um dado em um VP
 */
sl_status_t dwin_write_vp(uint16_t vp, const uint8_t *data, size_t size)
{
  if(data == NULL || size == 0)
    return SL_STATUS_NULL_POINTER;

  if(size > DWIN_MAX_DATA_LENGTH - 3U)
    return SL_STATUS_INVALID_PARAMETER;

  uint8_t tx[DWIN_MAX_PACKET_SIZE];

  size_t length = build_write_packet(tx, vp, data, size);

  if(length > sizeof(tx))
    return SL_STATUS_INVALID_PARAMETER;

  return usart_write(tx, length);
}

/*
 * Lê uma quantidade X de words de um VP
 */
sl_status_t dwin_read_vp(uint16_t vp, uint8_t words)
{
  if(words == 0U)
    return SL_STATUS_INVALID_PARAMETER;

  uint8_t tx[7];

  size_t length = build_read_packet(tx, vp, words);

  return usart_write(tx, length);
}

/*
 * Monta um pacote de Escrita para enviar ao display
 */
static size_t build_write_packet(uint8_t *tx, uint16_t vp, const uint8_t *data, uint8_t data_size)
{
  size_t index = 0;

  tx[index++] = DWIN_HEADER_1;
  tx[index++] = DWIN_HEADER_2;
  tx[index++] = 3 + data_size;
  tx[index++] = DWIN_CMD_WRITE;
  tx[index++] = (uint8_t)(vp >> 8);
  tx[index++] = (uint8_t)(vp & 0xFF);

  memcpy(&tx[index], data, data_size);

  index += data_size;

  return index;
}

/*
 * Monta um pacote de Leitura para enviar ao display
 */
static size_t build_read_packet(uint8_t *tx, uint16_t vp, uint8_t words)
{
  tx[0] = DWIN_HEADER_1;
  tx[1] = DWIN_HEADER_2;
  tx[2] = 0x04;
  tx[3] = DWIN_CMD_READ;
  tx[4] = (uint8_t) (vp >> 8);
  tx[5] = (uint8_t) (vp);
  tx[6] = words;

  return 7;
}
