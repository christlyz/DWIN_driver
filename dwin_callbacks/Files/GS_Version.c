/*
 * GS_Version.c
 *
 *  Created on: 8 de out. de 2026
 *      Author: christian.santos
 */

#include <stdint.h>
#include <stddef.h>

#include "GS_Version.h"

const char file_gs_version_name[] = "gs_version.txt";

const uint8_t file_gs_version_data[] = {
    0x45, 0x46, 0x41, 0x53, 0x5F, // EFAS_
    0x30, 0x31,             // 01
    0x2E,                   // .
    0x30, 0x30,             // 00
};

// "EFAS_01.00  "
const size_t file_gs_version_data_size = sizeof(file_gs_version_data);

static dwin_update_file_t file_gs_version =
{
    .name = file_gs_version_name,
    .data = file_gs_version_data,
    .size = file_gs_version_data_size,
    .position = 0U
};

dwin_update_file_t *get_file_gs_version(void)
{
    return &file_gs_version;
}
