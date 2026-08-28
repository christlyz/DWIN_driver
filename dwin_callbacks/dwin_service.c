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
#include "dwin_service.h"

/*******************************************************************************
 * Data types
 ******************************************************************************/
typedef struct dwin_callback_entry
{
  uint16_t vp;
  uint8_t instruction;
  uint16_t expected_data;
  dwin_vp_callback_t callback;
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

static uint8_t rx_buffer[DWIN_MAX_PACKET_SIZE];
static size_t rx_count = 0;

static dwin_config_t *dwin;
/*******************************************************************************
 * Extern
 ******************************************************************************/

/*******************************************************************************
 * Private Function Prototypes
 ******************************************************************************/
static void dwin_handle_received_vp(uint16_t vp, uint8_t instruction, const uint8_t *data, size_t size, void *context);
static void dwin_dispatch_received_vp(uint16_t vp, uint8_t instruction, const uint8_t *data, size_t size, void *context);
static void device_configuration_callback(sl_status_t status, uint16_t vp, const uint8_t *data, size_t data_size, void *context);
static void standby_handle(bool activated, uint8_t *settings);
static void touch_sound_handle(bool activated, uint8_t *settings);
static sl_status_t dwin_config_brightness(uint8_t default_brightness, uint8_t standby_brightness, uint16_t backlight_delay_ms);
static bool dwin_receive_bytes(void);
static void dwin_process_packets(void);
static void process_packet(const uint8_t *packet, size_t packet_size);
static void parse_read(const uint8_t *packet, size_t packet_size);
static void parse_write(const uint8_t *packet, size_t packet_size);
static void parse_ack(const uint8_t *packet);
static uint16_t bytes_to_u16(uint8_t msb, uint8_t lsb);
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
/*
 * Retorna a instância das configurações da DWIN
 */
dwin_config_t* dwin_get_config()
{
  dwin = (dwin_config_t*) malloc(sizeof(dwin_config_t));

  dwin->brightness = DWIN_DEFAULT_BRIGHTNESS;
  dwin->standby_brightness = DWIN_DEFAULT_STANDBY_BRIGHTNESS;
  dwin->standby_timeout = DWIN_DEFAULT_STANDBY_TIMEOUT;
  dwin->standby_brightness_activated = DWIN_DEFAULT_STANDBY_ACTIVATED;
  dwin->touch_sound_activated = DWIN_DEFAULT_TOUCH_SOUND_ACTIVATED;

  return dwin;
}

/*
 * Configura a DWIN com as configurações registradas na instancia dwin_config_t
 */
sl_status_t dwin_configure_device()
{
  if(dwin == NULL)
    return SL_STATUS_NULL_POINTER;

  sl_status_t status = dwin_config_brightness(dwin->brightness, dwin->standby_brightness, dwin->standby_timeout);
  if(status != SL_STATUS_OK)
    return status;

  status = dwin_read_vp_async(DWIN_VP_SYSTEM_CONFIG,
                            2,
                            5000,
                            device_configuration_callback);

  return status;
}

/*
 * Callback responsável por ativar/desativar o standby e o touch sound
 */
static void device_configuration_callback(sl_status_t status, uint16_t vp, const uint8_t *data, size_t data_size, void *context)
{
  if(status != SL_STATUS_OK)
    return;

  if(data == NULL || data_size < 4U)
    return;

  uint8_t settings = data[3];

  standby_handle(dwin->standby_brightness_activated, &settings);
  touch_sound_handle(dwin->touch_sound_activated, &settings);

  uint8_t new_data[4];

  new_data[0] = 0x5A;
  new_data[1] = 0x00;
  new_data[2] = 0x00;
  new_data[3] = settings;

  dwin_write_vp(DWIN_VP_SYSTEM_CONFIG, new_data, sizeof(new_data));
}

/*
 * Ativa/desativa o bit de configuração do standby
 */
static void standby_handle(bool activated, uint8_t *settings)
{
  if(activated)
    *settings |= DWIN_STANDBY_BIT_CONTROL;
  else
    *settings &= ~DWIN_STANDBY_BIT_CONTROL;
}

/*
 * Ativa/desativa o bit de configuração do touch sound
 */
static void touch_sound_handle(bool activated, uint8_t *settings)
{
  if(activated)
    *settings |= DWIN_TOUCH_SOUND_BIT_CONTROL;
  else
    *settings &= ~DWIN_TOUCH_SOUND_BIT_CONTROL;
}

/*
 * Configura o brilho atual, o brilho em standby e o tempo até standby (milissegundos)
 */
static sl_status_t dwin_config_brightness(uint8_t default_brightness, uint8_t standby_brightness, uint16_t backlight_delay_ms)
{
  if(default_brightness > 100 || standby_brightness > 100)
    return SL_STATUS_INVALID_PARAMETER;

  uint16_t standby_time = backlight_delay_ms / 10;

  uint8_t data[4];

  data[0] = default_brightness;
  data[1] = standby_brightness;
  data[2] = (uint8_t) (standby_time >> 8);
  data[3] = (uint8_t) standby_time;

  return dwin_write_vp(DWIN_VP_BRIGHTNESS, data, sizeof(data));
}

/*
 * Executa a cada loop de execução principal
 */
void dwin_poll()
{
  dwin_receive_bytes();
  dwin_process_packets();
}

sl_status_t dwin_set_icon(uint16_t vp, uint16_t icon)
{
  uint8_t data[2];
  data[0] = (uint8_t) (icon >> 8);
  data[1] = icon;

  sl_status_t status = dwin_write_vp(vp, data, sizeof(data));
  return status;
}

sl_status_t dwin_write_text(uint16_t vp, uint8_t max_text_size, const char *text)
{
  if(text == NULL)
    return SL_STATUS_NULL_POINTER;

  dwin_clear_text(vp, max_text_size);

  size_t length = strlen(text);

  if(length > max_text_size)
    length = max_text_size;

  sl_status_t status = dwin_write_vp(vp, (const uint8_t*) text, length);
  return status;
}

sl_status_t dwin_read_text(uint16_t vp, uint8_t expected_size, dwin_read_callback_t callback)
{
//  uint8_t words = (expected_size + 1U) / 2U;
  return dwin_read_vp_async(vp, expected_size, 1000, callback);
}

size_t dwin_extract_text(const uint8_t *data, size_t data_size, char *text, size_t text_size)
{
  if(data == NULL || text == NULL)
    return 0;

  if(text_size == 0)
    return 0;

  size_t i;

  for(i = 0; i < data_size && i < text_size - 1; i++)
    {
      if(i + 1 < data_size &&
          data[i] == ' ' &&
          data[i + 1] == ' ')
        {
          break;
        }
      text[i] = (char)data[i];
    }

  text[i] = '\0';

  return i;
}

sl_status_t dwin_clear_text(uint16_t vp, uint8_t text_size)
{
  uint8_t buffer[text_size];
  memset(buffer, ' ', sizeof(buffer));

  sl_status_t status = dwin_write_vp(vp, buffer, sizeof(buffer));
  return status;
}

sl_status_t dwin_write(uint16_t vp, size_t data_size, uint8_t *data)
{
  return dwin_write_vp(vp, data, data_size);
}

sl_status_t dwin_read(uint16_t vp, size_t data_size, dwin_read_callback_t callback)
{
  uint8_t words = (data_size + 1U) / 2U;
  return dwin_read_vp_async(vp, words, 1000, callback);
}

/*
 * Registra um callback baseado no vp, instrução e dado esperado. Não é permitido criar um callback com estes mesmos parâmetros
 */
sl_status_t dwin_register_callback(uint16_t vp, uint8_t instruction, uint16_t expected_data, dwin_vp_callback_t callback)
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
  new_callback->expected_data = expected_data;
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
              current->instruction == instruction &&
              current->expected_data == expected_data){
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

/*
 * Remove um callback baseado no vp, instrução e dado esperado
 */
sl_status_t dwin_unregister_callback(uint16_t vp, uint8_t instruction, uint16_t expected_data)
{
  if(callbackList == NULL)
    return SL_STATUS_NOT_FOUND;

  if(callbackList->vp == vp &&
      callbackList->instruction == instruction &&
      callbackList->expected_data == expected_data)
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
          current->instruction == instruction &&
          current->next->expected_data == expected_data)
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

/*
 * Registra a requisição de leitura de um VP em no máximo X tempo (tempo deve ser informado em milissegundos), ao obter uma resposta da DWIN o callback será chamado
 */
sl_status_t dwin_read_vp_async(uint16_t vp, uint8_t words, uint32_t timeout_ms, dwin_read_callback_t callback)
{
  if(callback == NULL)
    return SL_STATUS_NULL_POINTER;

  if(words == 0U || words > 123)
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

/*
 * Cancela uma requisição de leitura de um VP
 */
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
 * Troca a página atual da DWIN
 */
sl_status_t dwin_change_page(uint16_t page)
{
  uint8_t data[4];

  data[0] = DWIN_PAGE_ENABLE;
  data[1] = DWIN_PAGE_SWITCH;
  data[2] = (uint8_t) (page >> 8);
  data[3] = (uint8_t) page;

  return dwin_write_vp(DWIN_VP_PAGE, data, sizeof(data));
}

/*
 * Toca o buzzer por X milissegundos
 */
sl_status_t dwin_play_buzzer_ms(uint16_t milliseconds)
{
  if((milliseconds / 8) > 0xFF)
    return SL_STATUS_INVALID_PARAMETER;

  uint8_t data[2];

  data[0] = 0x00;
  data[1] = (uint8_t) (milliseconds / 8);

  return dwin_write_vp(DWIN_VP_BUZZER, data, sizeof(data));
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
 * Recebe uma resposta da DWIN e verifica se é uma requisição de resposta ou uma resposta direta da DWIN
 */
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

/*
 * Trata a resposta como um callback não requisitado
 */
static void dwin_dispatch_received_vp(uint16_t vp, uint8_t instruction, const uint8_t *data, size_t size, void *context)
{
  if(data == NULL)
      return;

  dwin_callback_entry_t *current_callback = callbackList;
  uint16_t received_data = bytes_to_u16(data[0], data[1]);

  while(current_callback != NULL)
    {
      if(current_callback->callback != NULL &&
          current_callback->vp == vp &&
          current_callback->instruction == instruction &&
          current_callback->expected_data == received_data)
        {
          current_callback->callback(vp, data, size, context);
          return;
        }
      current_callback = current_callback->next;
    }
}

/*
 * Verifica timeout de todas as exceções
 */
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

/*
 * Callback de evento para verificar timeout de requisição
 */
static void check_timeout_handler(sl_zigbee_event_t *event)
{
  dwin_process_timeout();
  sl_zigbee_event_set_delay_ms(&check_timeout_event, 100);
}

/*
 * Verifica se o evento de timeout foi iniciado
 */
static void check_timeout_init()
{
  if(is_timeout_initialized)
    return;

  sl_zigbee_event_init(&check_timeout_event, check_timeout_handler);
  is_timeout_initialized = true;
}

/*
 * Inicia o evento de timeout
 */
static void start_timeout()
{
  sl_zigbee_event_set_delay_ms(&check_timeout_event, 100);
}

/*
 * Para o evento de timeout
 */
static void stop_timeout()
{
  sl_zigbee_event_set_inactive(&check_timeout_event);
}

static uint16_t bytes_to_u16(uint8_t msb, uint8_t lsb)
{
  return (((uint16_t) msb << 8) | lsb);
}
