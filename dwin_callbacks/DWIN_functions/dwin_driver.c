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
static size_t build_write_packet_crc(uint8_t *tx, uint16_t vp, const uint8_t *data, size_t data_size);
static size_t build_read_packet(uint8_t *tx, uint16_t vp, uint8_t words);
static size_t build_read_packet_crc(uint8_t *tx, uint16_t vp, uint8_t words);
static uint16_t dwin_crc16_calculate(uint16_t crc, uint8_t data);
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

sl_status_t dwin_write_vp_crc(uint16_t vp, const uint8_t * data, size_t size)
{
  uint8_t tx[DWIN_MAX_PACKET_SIZE];
  size_t length;

  if(data == NULL || size == 0U)
    {
      return SL_STATUS_NULL_POINTER;
    }

  if(size > DWIN_MAX_DATA_LENGTH - 5U)
    {
      return SL_STATUS_INVALID_PARAMETER;
    }

  length = build_write_packet_crc(tx, vp, data, size);

  if(length == 0U || length > sizeof(tx))
    {
      return SL_STATUS_INVALID_PARAMETER;
    }

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

  if(length == 0U || length > sizeof(tx))
    {
      return SL_STATUS_INVALID_PARAMETER;
    }

  return usart_write(tx, length);
}

sl_status_t dwin_read_vp_crc(uint16_t vp, uint8_t words)
{
  if(words == 0U)
    {
      return SL_STATUS_INVALID_PARAMETER;
    }
  uint8_t tx[9];
  size_t length = build_read_packet_crc(tx, vp, words);

  if(length == 0U || length > sizeof(tx))
    {
      return SL_STATUS_INVALID_PARAMETER;
    }

  return usart_write(tx, length);
}

bool dwin_packet_check_crc(const uint8_t *packet, size_t packet_size)
{
  uint16_t calculated_crc = 0xFFFFU;
  uint16_t received_crc;
  size_t data_size;
  size_t i;

  if(packet == NULL)
    {
      return false;
    }

  if(packet_size < DWIN_HEADER_SIZE + 1U + DWIN_CRC_SIZE)
    {
      return false;
    }

  if(packet[0] != DWIN_HEADER_1 || packet[1] != DWIN_HEADER_2)
    {
      return false;
    }

  if(packet_size != (size_t)(DWIN_HEADER_SIZE + packet[2]))
    {
      return false;
    }

  if(packet[2] < (1U + DWIN_CRC_SIZE))
    {
      return false;
    }

  data_size = packet_size - DWIN_HEADER_SIZE - DWIN_CRC_SIZE;

  for(i = 0U; i < data_size; i++)
    {
      calculated_crc = dwin_crc16_calculate(calculated_crc, packet[DWIN_HEADER_SIZE + i]);
    }

  received_crc = ((uint16_t)packet[packet_size - 1U] << 8) | (uint16_t)packet[packet_size - 2U];

  return calculated_crc == received_crc;
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

static size_t build_write_packet_crc(uint8_t *tx, uint16_t vp, const uint8_t *data, size_t data_size)
{
  uint16_t crc = 0xFFFFU;
  size_t index = 0U;
  size_t i;

  tx[index++] = DWIN_HEADER_1;
  tx[index++] = DWIN_HEADER_2;
  tx[index++] = 5 + data_size;
  tx[index++] = DWIN_CMD_WRITE;
  tx[index++] = (uint8_t)(vp >> 8);
  tx[index++] = (uint8_t)(vp & 0xFF);

  memcpy(&tx[index], data, data_size);

  index += data_size;

  for(i = DWIN_HEADER_SIZE; i < index; i++)
    {
      crc = dwin_crc16_calculate(crc, tx[i]);
    }

  tx[index++] = (uint8_t)(crc & 0xFFU);
  tx[index++] = (uint8_t)(crc >> 8);

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

static size_t build_read_packet_crc(uint8_t *tx, uint16_t vp, uint8_t words)
{
  uint16_t crc = 0xFFFFU;
  size_t index = 0U;
  size_t i;

  tx[index++] = DWIN_HEADER_1;
  tx[index++] = DWIN_HEADER_2;

  tx[index++] = 0x06U;

  tx[index++] = DWIN_CMD_READ;

  tx[index++] = (uint8_t)(vp >> 8);
  tx[index++] = (uint8_t)(vp & 0xFFU);

  tx[index++] = words;

  for(i = DWIN_HEADER_SIZE; i < index; i++)
    {
      crc = dwin_crc16_calculate(crc, tx[i]);
    }

  tx[index++] = (uint8_t)(crc & 0xFFU);
  tx[index++] = (uint8_t)(crc >> 8);

  return index;
}

static uint16_t dwin_crc16_calculate(uint16_t crc, uint8_t data)
{
  uint8_t bit;

  crc ^= (uint16_t)data;

  for(bit = 0U; bit < 8U; bit++)
    {
      if((crc & 0x0001U) != 0U)
        {
          crc >>= 1;
          crc ^= 0xA001U;
        }
      else
        {
          crc >>= 1;
        }
    }
  return crc;
}

