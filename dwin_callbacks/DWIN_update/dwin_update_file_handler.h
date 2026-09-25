/*******************************************************************************
 * File Name.h
 *
 * Created on: May 27, 2026
 * Author Christian dos Santos
 *
 ******************************************************************************/

/*******************************************************************************
 * Description
 *
 * Usage:
 * Known Errors:
 * ToDo:
 ******************************************************************************/

/*******************************************************************************
 * Multiple include protection
 ******************************************************************************/

#ifndef DWIN_UPDATE_FILE_HANDLER_H_
#define DWIN_UPDATE_FILE_HANDLER_H_

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "dwin_update_internal.h"
/*******************************************************************************
 * Macros
 ******************************************************************************/

/*******************************************************************************
 * Defines
 ******************************************************************************/
#define DWIN_UPDATE_BLOCKS_PER_FILE_ID_0XAA \
        (DWIN_UPDATE_FILE_ID_SIZE / DWIN_UPDATE_FLASH_BLOCK_SIZE_0XAA)

#define DWIN_UPDATE_FILE_ID_SIZE (256U * 1024U)
/*******************************************************************************
 * Typedef & Enums
 ******************************************************************************/

/*******************************************************************************
 * Interface Funtions
 ******************************************************************************/
/*
 * Responsável por identificar e armazenar a extensão do arquivo.
 */
bool dwin_update_extension_handler(dwin_update_t *update);

/*
 * Responsável por identificar o método de atualização associado
 * ao nome do arquivo.
 */
bool dwin_update_identify_file_handler(dwin_update_t *update);

/*
 * Responsável por validar e calcular as informações de tamanho
 * necessárias para a atualização.
 */
bool dwin_update_file_size_handler(sl_status_t *status, dwin_update_t *update);

/*
 * Responsável por validar se os IDs necessários para o arquivo
 * estão dentro da faixa suportada pela memória da DWIN.
 */
bool dwin_update_validate_file_id_range(dwin_update_t *update);

/*
 * Responsável por inicializar os valores internos usados pela
 * máquina de estados após a validação do arquivo.
 */
void dwin_update_init_values(dwin_update_t *update);
/*******************************************************************************
 * End
 ******************************************************************************/
#endif
