/*
 * dwin_update_transfer.c
 *
 *  Created on: 25 de set. de 2026
 *      Author: christian.santos
 */

#include "dwin_update_transfer.h"
#include "../DWIN_functions/dwin_service.h"

#include <stdio.h>
#include <string.h>
/*
 * Responsável por carregar o próximo trecho do arquivo no buffer
 * interno utilizado pela transferência para a DWIN.
 */
sl_status_t load_next_chunk(void)
{
  size_t data_size;
  size_t buffer_size;
  size_t bytes_read;
  size_t file_remaining;
  size_t block_remaining;

  update.buffer_size = 0U;
  update.buffer_offset = 0U;

  if(update.file_offset > update.file_size)
    {
      update.error = DWIN_UPDATE_ERROR_FILE_SIZE;
      return SL_STATUS_INVALID_STATE;
    }
  /*
   * O bloco atual já está completo
   */
  if(update.block_offset >= update.current_block_size)
    {
      return SL_STATUS_OK;
    }

  file_remaining = (size_t)(update.file_size - update.file_offset);

  block_remaining = (size_t)(update.current_block_size - update.block_offset);

  /*
   * Ainda existe conteúdo real do arquivo
   */
  if(file_remaining > 0U)
    {
      data_size = sizeof(update.buffer);

      if(data_size > file_remaining)
        {
          data_size = file_remaining;
        }

      if(data_size > block_remaining)
        {
          data_size = block_remaining;
        }

      if(data_size == 0U)
        {
          update.error = DWIN_UPDATE_ERROR_FILE_SIZE;
          return SL_STATUS_INVALID_PARAMETER;
        }

      /*
       * O protocolo trabalha com words.
       * Se o ultimo trecho real for impar,
       * acrescenta um byte 0x00
       */

      buffer_size = data_size;

      if((data_size & 1U) != 0U)
        {
          if(data_size >= sizeof(update.buffer) ||
              data_size >= block_remaining)
            {
              update.error = DWIN_UPDATE_ERROR_FILE_SIZE;
              return SL_STATUS_INVALID_PARAMETER;
            }

          buffer_size++;

          update.buffer[data_size] = DWIN_UPDATE_FILL_VALUE;
        }

      /*
       * file_offset é a posição lógica do arquivo.
       * O position da estrutura é sincronizado antes da leitura
       */

      update.file->position = (size_t)update.file_offset;

      bytes_read = 0U;

      if(!dwin_update_file_read(update.file,
                                update.buffer,
                                data_size,
                                &bytes_read))
        {
          update.error = DWIN_UPDATE_ERROR_FILE_READ;
          return SL_STATUS_FAIL;
        }

      /*
       * Aqui compara com os bytes realmente lidos
       * do arquivo, não com o tamanho após padding
       */
      if(bytes_read != data_size)
        {
          update.error = DWIN_UPDATE_ERROR_FILE_READ;
          return SL_STATUS_IO;
        }

      update.buffer_size = buffer_size;
      update.buffer_offset = 0U;

      return SL_STATUS_OK;
    }

  /*
   * O arquivo terminou.
   * Completa o restante do bloco físico com 0x00
   */
  buffer_size = sizeof(update.buffer);

  if(buffer_size > block_remaining)
    {
      buffer_size = block_remaining;
    }

  /*
   * O bloco 0xAA possui tamanho par e o offset
   * também deve permanecer alinhado em words
   */
  if((buffer_size & 1U) != 0U)
    {
      buffer_size--;
    }

  if(buffer_size == 0U)
    {
      update.error = DWIN_UPDATE_ERROR_FILE_SIZE;
      return SL_STATUS_INVALID_PARAMETER;
    }

  memset(update.buffer,
         DWIN_UPDATE_FILL_VALUE,
         buffer_size);

  update.buffer_size = buffer_size;
  update.buffer_offset = 0U;

  return SL_STATUS_OK;
}

/*
 * Responsável por enviar um pacote do buffer para a RAM da DWIN
 * e aguardar sua confirmação através do mecanismo assíncrono.
 */
sl_status_t send_buffer_to_ram(void)
{
  size_t packet_size;

  if(update.buffer_offset >= update.buffer_size)
    {
      return SL_STATUS_INVALID_STATE;
    }

  packet_size = update.buffer_size - update.buffer_offset;

  if(packet_size > DWIN_UPDATE_PACKET_SIZE)
    {
      packet_size = DWIN_UPDATE_PACKET_SIZE;
    }

  if((packet_size & 1U) != 0U)
    {
      packet_size--;
    }

  if(packet_size == 0U)
    {
      return SL_STATUS_INVALID_PARAMETER;
    }

  update.current_packet_size = packet_size;

  return dwin_write_vp_async(
      update.ram_address,
      &update.buffer[update.buffer_offset],
      packet_size,
      DWIN_UPDATE_RAM_WRITE_TIMEOUT_MS,
      dwin_update_ram_write_ack_callback,
      NULL);
}

/*
 * Responsável por processar o resultado do ACK de uma escrita
 * na RAM e decidir se deve repetir, continuar ou iniciar a gravação
 * do bloco na Flash.
 */
void dwin_update_ram_write_ack_callback(sl_status_t status, uint16_t vp, void *context)
{
  (void)vp;
  (void)context;

  size_t file_remaining;
  size_t file_bytes_acked;

  if(status != SL_STATUS_OK)
    {
      if(dwin_update_is_recoverable_error(status) &&
          update.ram_retry_count < DWIN_UPDATE_MAX_RETRIES)
        {
          update.ram_retry_count++;

          printf("Falha escrita RAM - retry %u/%u\r\n", update.ram_retry_count, DWIN_UPDATE_MAX_RETRIES);

          update.state = DWIN_UPDATE_STATE_WRITE_RAM;
          return;
        }

      update.error = DWIN_UPDATE_ERROR_DWIN_WRITE;
      update.state = DWIN_UPDATE_STATE_ERROR;
      return;
    }

  file_remaining = 0U;

  if(update.file_offset < update.file_size)
    {
      file_remaining = (size_t)(update.file_size - update.file_offset);
    }

  file_bytes_acked = update.current_packet_size;

  if(file_bytes_acked > file_remaining)
    {
      file_bytes_acked = file_remaining;
    }

  update.file_offset += (uint32_t)file_bytes_acked;
  update.buffer_offset += update.current_packet_size;
  update.block_offset += (uint32_t)update.current_packet_size;

  /*
   * Cada palavra ocupa 2 bytes no VP
   */
  update.ram_address += (uint16_t)(update.current_packet_size / 2U);
  update.ram_retry_count = 0U;

  dwin_update_calculate_progress();
  /*
   * Ainda há dados no buffer
   */
  if(update.buffer_offset < update.buffer_size)
    {
      update.state = DWIN_UPDATE_STATE_WRITE_RAM;
      return;
    }

  /*
   * O buffer atual terminou, mas o bloco de 32Kib ainda não
   */
  if(update.block_offset < update.current_block_size)
    {
      update.state = DWIN_UPDATE_STATE_LOAD_BLOCK;
      return;
    }

  update.state = DWIN_UPDATE_STATE_FLASH_WRITE;
}
