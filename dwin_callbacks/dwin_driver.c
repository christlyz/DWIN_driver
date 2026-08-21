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
static uint8_t rx_buffer[DWIN_MAX_PACKET_SIZE];
static size_t rx_count = 0;

typedef struct dwin_callback_entry
{
  uint16_t vp;
  uint8_t instruction;
  dwin_vp_callback_t callback;
  uint16_t expected_data;
  struct dwin_callback_entry *next;
} dwin_callback_entry_t;

typedef struct dwin_pending_read
{
  uint16_t vp;
  uint32_t timeout_ms;
  uint32_t start_tick;
  dwin_read_callback_t callback;
  struct dwin_pending_read *next;
} dwin_pending_read_t;

static dwin_callback_entry_t *callbackList;
static dwin_pending_read_t *pending_read;

static sl_zigbee_event_t check_timeout_event;
static void check_timeout_handler(sl_zigbee_event_t *event);

static bool is_timeout_initialized = false;
/*******************************************************************************
 * Extern
 ******************************************************************************/

/*******************************************************************************
 * Private Function Prototypes
 ******************************************************************************/
static bool dwin_receive_bytes(void);
static void dwin_process_packets(void);
static void parse_read(const uint8_t *packet, size_t packet_size);
static void parse_write(const uint8_t *packet, size_t packet_size);
static void parse_ack(const uint8_t *packet);
static size_t build_write_packet(uint8_t *tx, uint16_t vp, const uint8_t *data, uint8_t data_size);
static size_t build_read_packet(uint8_t *tx, uint16_t vp, uint8_t words);
static uint16_t bytes_to_u16(uint8_t msb, uint8_t lsb);
static void process_packet(const uint8_t *packet, size_t packet_size);
static void dwin_handle_received_vp(uint16_t vp, uint8_t instruction, const uint8_t *data, size_t size, void *context);
static void dwin_dispatch_received_vp(uint16_t vp, uint8_t instruction, const uint8_t *data, size_t size, void *context);
static void dwin_process_timeout();
static void check_timeout_init();
static void start_timeout();
static void stop_timeout();
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
sl_status_t dwin_register_callback(uint16_t vp, uint8_t instruction, dwin_vp_callback_t callback)
{
  if(callback == NULL){
      return SL_STATUS_NULL_POINTER;
  }

  dwin_callback_entry_t *new_callback = (dwin_callback_entry_t*) malloc(sizeof(dwin_callback_entry_t));

  if(new_callback == NULL){
      return SL_STATUS_ALLOCATION_FAILED;
  }

  new_callback->vp = vp;
  new_callback->instruction = instruction;
  new_callback->callback = callback;
  new_callback->next = NULL;

  if(callbackList == NULL)
  {
      callbackList = new_callback;
      return SL_STATUS_OK;
  }
  else{
      dwin_callback_entry_t *current = callbackList;
      while(current != NULL)
       {
          if(current->vp == vp &&
              current->instruction == instruction){
              free(new_callback);
              return SL_STATUS_ALREADY_EXISTS;
          }
          if(current->next == NULL)
            {
              break;
            }
          current = current->next;
       }
      current->next = new_callback;
  }

  return SL_STATUS_OK;
}

sl_status_t dwin_unregister_callback(uint16_t vp, uint8_t instruction)
{
  if(callbackList == NULL)
    return SL_STATUS_NOT_FOUND;

  if(callbackList->vp == vp &&
      callbackList->instruction == instruction)
    {
      dwin_callback_entry_t *remove_callback = callbackList;

      callbackList = callbackList->next;
      free(remove_callback);
      return SL_STATUS_OK;
    }

  dwin_callback_entry_t *current = callbackList;

  while(current->next != NULL)
    {
      if(current->next->vp == vp &&
          current->next->instruction == instruction)
        {
          dwin_callback_entry_t *remove_callback = current->next;
          current->next = remove_callback->next;
          free(remove_callback);
          return SL_STATUS_OK;
        }
      current = current->next;
    }

  return SL_STATUS_NOT_FOUND;
}

sl_status_t dwin_read_vp_async(uint16_t vp, uint8_t words, uint32_t timeout_ms, dwin_read_callback_t callback)
{
  if(callback == NULL)
    return SL_STATUS_NULL_POINTER;

  if(words == 0U)
    return SL_STATUS_INVALID_PARAMETER;

  dwin_pending_read_t *new_pending_read = (dwin_pending_read_t*) malloc(sizeof(dwin_pending_read_t));

  if(new_pending_read == NULL)
    return SL_STATUS_ALLOCATION_FAILED;

  check_timeout_init();

  dwin_pending_read_t *current = pending_read;

  while(current != NULL)
    {
      if(current->vp == vp)
        {
          free(new_pending_read);
          return SL_STATUS_ALREADY_EXISTS;
        }

      current = current->next;
    }

  sl_status_t status = dwin_read_vp(vp, words);

  if(status != SL_STATUS_OK)
    {
      free(new_pending_read);
      return status;
    }

  new_pending_read->vp = vp;
  new_pending_read->timeout_ms = timeout_ms;
  new_pending_read->start_tick = sl_sleeptimer_get_tick_count();
  new_pending_read->callback = callback;
  new_pending_read->next = NULL;

  if(pending_read == NULL)
    {
      pending_read = new_pending_read;
      start_timeout();
    }
  else
    {
      current = pending_read;

      while(current->next != NULL)
        {
          current = current->next;
        }
      current->next = new_pending_read;
    }


  return SL_STATUS_OK;
}

sl_status_t dwin_cancel_read_vp(uint16_t vp)
{
  if(pending_read == NULL)
    return SL_STATUS_NOT_FOUND;

  if(pending_read->vp == vp)
    {
      dwin_pending_read_t *remove_pending = pending_read;

      pending_read = pending_read->next;
      free(remove_pending);

      if(pending_read == NULL)
        stop_timeout();

      return SL_STATUS_OK;
    }

  dwin_pending_read_t *current = pending_read;

  while(current->next != NULL)
    {
      if(current->next->vp == vp)
        {
          dwin_pending_read_t *remove_callback = current->next;
          current->next = remove_callback->next;
          free(remove_callback);

          return SL_STATUS_OK;
        }
      current = current->next;
    }

  return SL_STATUS_NOT_FOUND;
}

/*
 * Executa a cada loop de execução principal
 */
void dwin_poll()
{
  dwin_receive_bytes();
  dwin_process_packets();
}

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
 * Realiza leitura da USART, e salva num buffer temporário, se tiver algum dado, copia para o buffer principal
 */
static bool dwin_receive_bytes(void)
{
  uint8_t temp[32];
  size_t received = 0;

  sl_status_t status;

  status = usart_read(temp, sizeof(temp), &received);

  if(status != SL_STATUS_OK)
    {
      return false;
    }

  if(received == 0)
    {
      return false;
    }

  if(rx_count + received > sizeof(rx_buffer))
    {
      rx_count = 0;
      return false;
    }

  memcpy(&rx_buffer[rx_count], temp, received);

  rx_count += received;

  return true;
}

/*
 * Identifica e processa os pacotes do buffer principal
 */
static void dwin_process_packets(void)
{
  while(1)
    {
      //Minimo de bytes para processar = 3
      if(rx_count < DWIN_HEADER_SIZE)
        return;

      //Procura um cabeçalho na posição 0 e 1 do buffer
      if(rx_buffer[0] !=  DWIN_HEADER_1 || rx_buffer[1] != DWIN_HEADER_2)
        {
          memmove(rx_buffer, &rx_buffer[1], rx_count - 1);
          rx_count--;
          continue;
        }

      uint8_t length = rx_buffer[2];

      //Length do pacote tem que ser maior que 0 e no máximo 249
      if(length == 0U || length > DWIN_MAX_DATA_LENGTH)
        {
          memmove(rx_buffer, &rx_buffer[1], rx_count - 1);
          rx_count--;
          continue;
        }

      size_t packet_size = DWIN_HEADER_SIZE + length;

      if(rx_count < packet_size)
        return;

      process_packet(rx_buffer, packet_size);

      size_t remaining = rx_count - packet_size;

      if(remaining > 0)
        {
          memmove(rx_buffer, &rx_buffer[packet_size], remaining);
        }

      rx_count = remaining;
    }
}

/*
 * Processa um pacote para enviar ao parser
 */
static void process_packet(const uint8_t *packet, size_t packet_size)
{
  if(packet == NULL || packet_size < DWIN_HEADER_SIZE + 1U)
    return;

  uint8_t length = packet[2];

  if(packet_size != (DWIN_HEADER_SIZE + length))
    return;

  uint8_t instruction = packet[3];
  switch(instruction)
  {
    case DWIN_CMD_READ:
      {
        parse_read(packet, packet_size);
        break;
      }

    case DWIN_CMD_WRITE:
      {
        if(packet_size == 6U &&
            packet[4] == DWIN_WRITE_OK_1 &&
            packet[5] == DWIN_WRITE_OK_2)
          {
            parse_ack(packet);
            break;
          }

        parse_write(packet, packet_size);
        break;
      }

    default:
      break;
  }
}

/*
 * Separa os bits de um pacote de leitura de acordo com o seu significado
 */
static void parse_read(const uint8_t *packet, size_t packet_size)
{
  if(packet == NULL || packet_size < 7U)
    return;

  uint8_t words = packet[6];
  size_t data_size = (size_t) words * 2U;

  if(packet_size != 7U + data_size)
    return;

  uint8_t instruction = DWIN_CMD_READ;
  uint16_t vp = bytes_to_u16(packet[4], packet[5]);

  if(vp == 0x3000)
    return;

  dwin_handle_received_vp(vp, instruction, &packet[7], data_size, NULL);
}

/*
 * Separa os bits de um pacote de escrita de acordo com o seu significado
 */
static void parse_write(const uint8_t *packet, size_t packet_size)
{
  if(packet == NULL || packet_size < 7U)
    return;

  uint8_t instruction = DWIN_CMD_WRITE;
  uint16_t vp = bytes_to_u16(packet[4], packet[5]);
  size_t data_size = packet_size - 6U;

  dwin_handle_received_vp(vp, instruction, &packet[6], data_size, NULL);
}

static void parse_ack(const uint8_t *packet)
{
  /*
   * Expansível para implementação de tratamento de ACK
   */
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

static uint16_t bytes_to_u16(uint8_t msb, uint8_t lsb)
{
  return (((uint16_t) msb << 8) | lsb);
}

static void dwin_handle_received_vp(uint16_t vp, uint8_t instruction, const uint8_t *data, size_t size, void *context)
{
  if(pending_read != NULL)
    {
      dwin_pending_read_t *current_pending = pending_read;

      while(current_pending != NULL)
        {
          if(current_pending->vp == vp)
            {
              dwin_read_callback_t read_callback = current_pending->callback;
              dwin_cancel_read_vp(vp);
              read_callback(SL_STATUS_OK, vp, data, size, context);
              return;
            }
          current_pending = current_pending->next;
        }
    }

  dwin_dispatch_received_vp(vp, instruction, data, size, context);
}

static void dwin_dispatch_received_vp(uint16_t vp, uint8_t instruction, const uint8_t *data, size_t size, void *context)
{
  if(data == NULL)
      return;

  dwin_callback_entry_t *current_callback = callbackList;
  while(current_callback != NULL)
    {
      if(current_callback->callback != NULL &&
          current_callback->vp == vp &&
          current_callback->instruction == instruction)
        {
          current_callback->callback(vp, data, size, context);
          return;
        }
      current_callback = current_callback->next;
    }
}

static void dwin_process_timeout()
{
  if(pending_read == NULL)
    return;

  uint32_t now = sl_sleeptimer_get_tick_count();
  uint32_t elapsed_time;

  dwin_pending_read_t *current_pending = pending_read;

  while(current_pending != NULL)
    {
      elapsed_time = sl_sleeptimer_tick_to_ms(now - current_pending->start_tick);
      dwin_pending_read_t *next_pending = current_pending->next;
      if(elapsed_time >= current_pending->timeout_ms)
        {
          uint16_t vp = current_pending->vp;
          dwin_read_callback_t read_callback = current_pending->callback;
          dwin_cancel_read_vp(vp);
          read_callback(SL_STATUS_TIMEOUT, vp, NULL, 0, NULL);
        }
      current_pending = next_pending;
    }
}

static void check_timeout_handler(sl_zigbee_event_t *event)
{
  dwin_process_timeout();
  sl_zigbee_event_set_delay_ms(&check_timeout_event, 100);
}

static void check_timeout_init()
{
  if(is_timeout_initialized)
    return;

  sl_zigbee_event_init(&check_timeout_event, check_timeout_handler);
  is_timeout_initialized = true;
}

static void start_timeout()
{
  sl_zigbee_event_set_delay_ms(&check_timeout_event, 100);
}

static void stop_timeout()
{
  sl_zigbee_event_set_inactive(&check_timeout_event);
}
