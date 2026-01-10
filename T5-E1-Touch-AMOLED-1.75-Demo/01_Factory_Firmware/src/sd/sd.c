#include "sd.h"

#include "tal_api.h"

bool sd_init()
{
    OPERATE_RET rt = OPRT_OK;
    TUYA_CALL_ERR_LOG(tkl_fs_mount(SDCARD_MOUNT_PATH, DEV_SDCARD));
    if (rt != OPRT_OK) {
        PR_ERR("Mount SD card failed: %d", rt);
        return false;
    }
    return true;
}

void sd_test()
{

}