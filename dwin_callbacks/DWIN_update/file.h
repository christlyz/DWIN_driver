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

#ifndef FILE_H_
#define FILE_H_

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <string.h>
#include "sl_status.h"
#include "dwin_update.h"
#include "dwin_file.h"
#include "dwin_version.h"

#include "../DWIN_functions/dwin_service.h"

#include "../Files/13TouchFile.h"
#include "../Files/14ShowFile.h"
//#include "../Files/22Config.h"
#include "../Files/GS_Version.h"
//#include "../Files/5.h"
//#include "../Files/60.h"
//#include "../Files/62.h"
/*******************************************************************************
 * Macros
 ******************************************************************************/

/*******************************************************************************
 * Defines
 ******************************************************************************/

/*******************************************************************************
 * Typedef & Enums
 ******************************************************************************/
typedef struct file_node
{
  dwin_update_file_t *file;
  struct file_node *next;
} file_node_t;

typedef enum
{
  FILE_UPDATE_STATE_INIT,
  FILE_UPDATE_STATE_FIND_VERSION,
  FILE_UPDATE_STATE_READ_VERSION_NOR,
  FILE_UPDATE_STATE_WAIT_VERSION_NOR,
  FILE_UPDATE_STATE_READ_VERSION_VP,
  FILE_UPDATE_STATE_WAIT_VERSION_VP,
  FILE_UPDATE_STATE_COMPARE_VERSION,
  FILE_UPDATE_STATE_START_FILE,
  FILE_UPDATE_STATE_WAIT_FILE,
  FILE_UPDATE_STATE_NEXT_FILE,
  FILE_UPDATE_STATE_SAVE_VERSION,
  FILE_UPDATE_STATE_WAIT_VERSION_SAVE,
  FILE_UPDATE_STATE_NO_UPDATE,
  FILE_UPDATE_STATE_RESET,
  /*
   * Futura recuperacao atraves de arquivo backup
   */
  FILE_UPDATE_STATE_RECOVERY,
  FILE_UPDATE_STATE_FINISH,
  FILE_UPDATE_STATE_ERROR
} files_update_state_t;

//file_node_t *file_list;
/*******************************************************************************
 * Interface Funtions
 ******************************************************************************/
void file_update_set_force(bool force);
void file_update_init(void);
sl_status_t start_update();
sl_status_t file_list_add(dwin_update_file_t *file);
void file_list_clear(void);
sl_status_t file_list_build(void);
/*******************************************************************************
 * End
 ******************************************************************************/
#endif
