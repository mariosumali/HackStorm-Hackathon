/**
 * @file tuya_main.c
 * @brief Voice Recorder Application - Main Entry Point
 * @version 1.0.0
 * @copyright Copyright (c) 2025
 */

#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "board_com_api.h"

#include "voice_recorder.h"
#include "gui_display.h"

/***********************************************************
************************macro define************************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/

/***********************************************************
***********************function define**********************
***********************************************************/

/**
 * @brief user_main - Main application function
 * @return int
 */
int user_main(void)
{
    OPERATE_RET rt = OPRT_OK;

    // Initialize logging
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 1024, (TAL_LOG_OUTPUT_CB)tkl_log_output);

    // Print application information
    PR_NOTICE("========================================");
    PR_NOTICE("       VOICE RECORDER APPLICATION      ");
    PR_NOTICE("========================================");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);
    PR_NOTICE("========================================");

    // Register hardware (display, audio, etc.)
    rt = board_register_hardware();
    if (rt != OPRT_OK) {
        PR_ERR("board_register_hardware failed: %d", rt);
        return rt;
    }
    PR_NOTICE("Hardware registered successfully");

    // Initialize voice recorder
    rt = voice_recorder_init();
    if (rt != OPRT_OK) {
        PR_ERR("voice_recorder_init failed: %d", rt);
        return rt;
    }
    PR_NOTICE("Voice recorder initialized");

    // Initialize GUI display
    rt = gui_display_init();
    if (rt != OPRT_OK) {
        PR_ERR("gui_display_init failed: %d", rt);
        return rt;
    }
    PR_NOTICE("GUI display initialized");

    // Start GUI display task
    rt = gui_display_start();
    if (rt != OPRT_OK) {
        PR_ERR("gui_display_start failed: %d", rt);
        return rt;
    }
    PR_NOTICE("GUI display started");

    PR_NOTICE("Application started successfully!");
    PR_NOTICE("Touch the RECORD button to start recording.");

    // Main loop
    while (1) {
        // Process voice recorder (check for auto-stop, etc.)
        voice_recorder_process();

        // Sleep to prevent busy loop
        tal_system_sleep(50);
    }

    return 0;
}

#if OPERATING_SYSTEM == SYSTEM_LINUX
/**
 * @brief main - Linux entry point
 */
void main(int argc, char *argv[])
{
    user_main();
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief tuya_app_thread - Task thread for RTOS
 * @param arg - Parameters when creating a task
 */
static void tuya_app_thread(void *arg)
{
    (void)arg;
    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

/**
 * @brief tuya_app_main - RTOS entry point
 */
void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {8192, 4, "voice_recorder"};
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif
