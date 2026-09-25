/*
 * dwin_update_transfer.h
 *
 *  Created on: 25 de set. de 2026
 *      Author: christian.santos
 */

#ifndef DWIN_UPDATE_DWIN_UPDATE_TRANSFER_H_
#define DWIN_UPDATE_DWIN_UPDATE_TRANSFER_H_

#include "dwin_update_internal.h"

/*
 * Responsável por carregar o próximo trecho do arquivo no buffer
 * interno de transferência.
 */
sl_status_t load_next_chunk(void);

/*
 * Responsável por enviar um pacote do buffer para a área RAM da DWIN.
 */
sl_status_t send_buffer_to_ram(void);

/*
 * Responsável por tratar o ACK de uma escrita realizada na RAM da DWIN.
 */
void dwin_update_ram_write_ack_callback(sl_status_t status,
                                        uint16_t vp,
                                        void *context);

#endif /* DWIN_UPDATE_DWIN_UPDATE_TRANSFER_H_ */
