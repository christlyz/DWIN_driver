/*
 * dwin_version.h
 *
 *  Created on: 9 de out. de 2026
 *      Author: christian.santos
 */

#ifndef DWIN_UPDATE_DWIN_VERSION_H_
#define DWIN_UPDATE_DWIN_VERSION_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "sl_status.h"

#include "../DWIN_update/dwin_file.h"

#define GS_VERSION_PREFIX   "EFAS_"
#define GS_VERSION_PREFIX_SIZE    5U
#define GS_VERSION_FILE_SIZE_BYTES 10U

#define GS_VERSION_SIZE_BYTES     12U
#define GS_VERSION_SIZE_WORDS     6U

typedef struct
{
  uint8_t major;
  uint8_t minor;
} gs_version_t;

/*
 * Converte:
 *
 * EFAS_01.02
 *
 * para:
 *
 * major = 1
 * minor = 2
 *
 */

sl_status_t gs_version_parse(const uint8_t *data, size_t data_size, gs_version_t *version);

/*
 * Retorno:
 *
 * < 0 -> version_a < version_b
 * 0   -> version_a == version_b
 * > 0 -> version_a > version_b
 */
int8_t gs_version_compare(const gs_version_t *version_a, const gs_version_t *version_b);

#endif /* DWIN_UPDATE_DWIN_VERSION_H_ */
