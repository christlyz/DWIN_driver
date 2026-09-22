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
#include "dwin_file.h"

#include <string.h>
/*******************************************************************************
 * Data types
 ******************************************************************************/

/*******************************************************************************
 * Extern
 ******************************************************************************/

/*******************************************************************************
 * Private Function Prototypes
 ******************************************************************************/

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
bool dwin_update_file_open(dwin_update_file_t *file, const char *name, const uint8_t *data, size_t size)
{
  if(file == NULL ||
      name == NULL ||
      data == NULL ||
      size == 0U)
    {
      return false;
    }

  file->name = name;
  file->data = data;
  file->size = size;
  file->position = 0U;

  return true;
}

bool dwin_update_file_read(dwin_update_file_t *file, uint8_t *buffer, size_t size, size_t *bytes_read)
{
  size_t remaining;
  size_t to_copy;

  if(file == NULL ||
      buffer == NULL ||
      bytes_read == NULL)
    {
      return false;
    }

  *bytes_read = 0U;

  /*
   * EOF
   */
  if(file->position >= file->size)
    {
      return true;
    }

  remaining = file->size - file->position;

  to_copy = size;

  if(to_copy > remaining)
    {
      to_copy = remaining;
    }

  memcpy(buffer, &file->data[file->position], to_copy);

  file->position += to_copy;

  *bytes_read = to_copy;

  return true;
}
void dwin_update_file_close(dwin_update_file_t *file)
{
  if(file == NULL)
    {
      return;
    }

  file->name = NULL;
  file->data = NULL;
  file->size = 0U;
  file->position = 0U;
}
