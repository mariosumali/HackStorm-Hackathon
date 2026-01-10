

#ifndef _SD_H
#define _SD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tkl_fs.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define TASK_SD_PRIORITY THREAD_PRIO_2
#define TASK_SD_SIZE     4096

#define SDCARD_MOUNT_PATH "/sdcard"

bool sd_init();






#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
