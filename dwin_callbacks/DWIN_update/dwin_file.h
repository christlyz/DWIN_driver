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

#ifndef DWIN_FILE_H_
#define DWIN_FILE_H_

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*******************************************************************************
 * Macros
 ******************************************************************************/

/*******************************************************************************
 * Defines
 ******************************************************************************/

/*******************************************************************************
 * Typedef & Enums
 ******************************************************************************/
typedef struct
{
  const char *name;
  const uint8_t *data;
  size_t size;
  size_t position;
} dwin_update_file_t;
/*******************************************************************************
 * Interface Funtions
 ******************************************************************************/
/*
 * Responsável por abrir ou preparar a fonte do arquivo para leitura.
 */
bool dwin_update_file_open(dwin_update_file_t *file, const char *name, const uint8_t *data, size_t size);

/*
 * Responsável por ler uma quantidade de bytes a partir da posição
 * atual do arquivo.
 */
bool dwin_update_file_read(dwin_update_file_t *file, uint8_t *buffer, size_t size, size_t *bytes_read);

/*
 * Responsável por finalizar o uso do arquivo e liberar os recursos
 * associados à sua leitura.
 */
void dwin_update_file_close(dwin_update_file_t *file);
/*******************************************************************************
 * End
 ******************************************************************************/
#endif
