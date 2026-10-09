/*
 * dwin_version.c
 *
 *  Created on: 9 de out. de 2026
 *      Author: christian.santos
 */

#include "dwin_version.h"

static uint8_t gs_version_parse_two_digits(uint8_t tens,
                                           uint8_t units)
{
  return (uint8_t)(((tens - '0') * 10U) +
                   (units - '0'));
}

sl_status_t gs_version_parse(const uint8_t *data, size_t data_size, gs_version_t *version)
{
  if((data == NULL) || (version == NULL))
    {
      return SL_STATUS_NULL_POINTER;
    }

  if(data_size != GS_VERSION_SIZE_BYTES)
    {
      return SL_STATUS_INVALID_PARAMETER;
    }

  /*
   * Formato:
   *
   * EFAS01.00.00
   *
   * Posicao:
   *
   * 0 = E
   * 1 = F
   * 2 = A
   * 3 = S
   * 4 = '_'
   * 5 = major tens
   * 6 = major units
   * 7 = '.'
   * 8 = minor tens
   * 9 = minor units
   * 10 = ' '
   * 11 = ' '
   */

  if(memcmp(data, GS_VERSION_PREFIX, GS_VERSION_PREFIX_SIZE) != 0)
  {
    return SL_STATUS_INVALID_PARAMETER;
  }

  if((data[5] < '0') || (data[5] > '9') ||
     (data[6] < '0') || (data[6] > '9') ||
     (data[7] != '.') ||
     (data[8] < '0') || (data[8] > '9') ||
     (data[9] < '0') || (data[9] > '9') ||
     (data[10] != ' ') ||
     (data[11] != ' '))
  {
      return SL_STATUS_INVALID_PARAMETER;
  }

  version->major = gs_version_parse_two_digits(data[5], data[6]);

  version->minor = gs_version_parse_two_digits(data[8], data[9]);

  return SL_STATUS_OK;
}

int8_t gs_version_compare(const gs_version_t *version_a,
                          const gs_version_t *version_b)
{
  if((version_a == NULL) || (version_b == NULL))
  {
    return 0;
  }

  if(version_a->major > version_b->major)
  {
    return 1;
  }

  if(version_a->major < version_b->major)
  {
    return -1;
  }

  if(version_a->minor > version_b->minor)
  {
    return 1;
  }

  if(version_a->minor < version_b->minor)
  {
    return -1;
  }

  return 0;
}
