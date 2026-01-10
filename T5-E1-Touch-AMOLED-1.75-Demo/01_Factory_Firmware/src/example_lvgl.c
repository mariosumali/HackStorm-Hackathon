/**
 * @file example_lvgl.c
 * @brief LVGL (Light and Versatile Graphics Library) example for SDK.
 *
 * This file provides an example implementation of using the LVGL library with the Tuya SDK.
 * It demonstrates the initialization and usage of LVGL for graphical user interface (GUI) development.
 * The example covers setting up the display port, initializing LVGL, and running a demo application.
 *
 * The LVGL example aims to help developers understand how to integrate LVGL into their Tuya IoT projects for
 * creating graphical user interfaces on embedded devices. It includes detailed examples of setting up LVGL,
 * handling display updates, and integrating these functionalities within a multitasking environment.
 *
 * @note This example is designed to be adaptable to various Tuya IoT devices and platforms, showcasing fundamental LVGL
 * operations critical for GUI development on embedded systems.
 *
 * @copyright Copyright (c) 2021-2024 Tuya Inc. All Rights Reserved.
 *
 */

#include "tuya_cloud_types.h"

#include "tal_api.h"
#include "tkl_output.h"
#include "tkl_spi.h"
#include "tkl_system.h"

#include "lvgl.h"
#include "demos/lv_demos.h"
#include "examples/lv_examples.h"

#include "ui/ui.h"
/***********************************************************
*************************micro define***********************
***********************************************************/
#define TASK_GPIO_PRIORITY THREAD_PRIO_2
#define TASK_GPIO_SIZE     4096

/***********************************************************
***********************typedef define***********************
***********************************************************/

/***********************************************************
***********************variable define**********************
***********************************************************/
static THREAD_HANDLE sg_example_handle;
static THREAD_HANDLE sg_wifi_ble_handle;

/***********************************************************
***********************function define**********************
***********************************************************/
/**
 * @brief example task
 *
 * @param[in] param:Task parameters
 * @return none
 */
static void __wifi_ble_task(void *param)
{
    (void)param;
    //WIFI
    WIFI_Scan();

    //BLE
    // BLE_Scan();

    tal_thread_delete(sg_wifi_ble_handle);
    sg_wifi_ble_handle = NULL;
}

static void __example_task(void *param)
{
    (void)param;
    
    PR_NOTICE("Application information:");
    PR_NOTICE("Project name:        %s", PROJECT_NAME);
    PR_NOTICE("App version:         %s", PROJECT_VERSION);
    PR_NOTICE("Compile time:        %s", __DATE__);
    PR_NOTICE("TuyaOpen version:    %s", OPEN_VERSION);
    PR_NOTICE("TuyaOpen commit-id:  %s", OPEN_COMMIT);
    PR_NOTICE("Platform chip:       %s", PLATFORM_CHIP);
    PR_NOTICE("Platform board:      %s", PLATFORM_BOARD);
    PR_NOTICE("Platform commit-id:  %s", PLATFORM_COMMIT);

    
    //SD
    if(sd_init())
        lv_textarea_set_placeholder_text(ui_TSDCARD, "Mounted successfully");
    else
        lv_textarea_set_placeholder_text(ui_TSDCARD, "Mounting failed");
    
    //MIC SPK
    mic_spk_test();
    
    //RTC
    pcf85063a_init();

    //QMI
    qmi8658_dev_t qmi_dev;  
    qmi8658_init(&qmi_dev, QMI8658_ADDRESS_HIGH);
    qmi8658_set_accel_range(&qmi_dev, QMI8658_ACCEL_RANGE_8G);
    qmi8658_set_accel_odr(&qmi_dev, QMI8658_ACCEL_ODR_1000HZ);
    qmi8658_set_gyro_range(&qmi_dev, QMI8658_GYRO_RANGE_512DPS);
    qmi8658_set_gyro_odr(&qmi_dev, QMI8658_GYRO_ODR_1000HZ);
    
    qmi8658_set_accel_unit_mps2(&qmi_dev, true);
    qmi8658_set_gyro_unit_rads(&qmi_dev, true);
    
    qmi8658_set_display_precision(&qmi_dev, 4);

    //GPS
    // use 40/41 as uart2
    tkl_io_pinmux_config(TUYA_GPIO_NUM_40, TUYA_UART2_RX);
    tkl_io_pinmux_config(TUYA_GPIO_NUM_41, TUYA_UART2_TX);
    dev_uart_init(EXAMPLE_UART_PORT, EXAMPLE_UART_BAUDRATE);
    lc76g_init_uart(&lc76g_dev, EXAMPLE_UART_PORT, EXAMPLE_UART_BAUDRATE);

    //BAT
    dev_gpio_init(EXAMPLE_BAT_CHARGE_PIN, TUYA_GPIO_INPUT);
    dev_adc_init();

    //WIFI
    static THREAD_CFG_T thrd_param = {.priority = TASK_GPIO_PRIORITY, .stackDepth = TASK_GPIO_SIZE, .thrdname = "wifi_ble"};
    tal_thread_create_and_start(&sg_wifi_ble_handle, NULL, NULL, __wifi_ble_task, NULL, &thrd_param);

    //GPIO
    dev_sys_init();

    uint8_t sec = 0;
    while (1) {

        qmi8658_loop(&qmi_dev);

        dev_digital_read(EXAMPLE_BAT_CHARGE_PIN, &bat_charge);
        bat_v = dev_adc_read();
        if (bat_v > 4200) bat_v = 4200;

        if (sec == 10)
        {
            pcf85063a_loop();
            lc76g_get_data(&lc76g_dev);
            sec = 0;
        }
        sec++;
        tal_system_sleep(100);
    }
}
/**
 * @brief user_main
 *
 * @param[in] param:Task parameters
 * @return none
 */
void user_main(void)
{
    OPERATE_RET rt = OPRT_OK;
    /* basic init */
    tal_log_init(TAL_LOG_LEVEL_DEBUG, 4096, (TAL_LOG_OUTPUT_CB)tkl_log_output);
    tal_sw_timer_init();

    /*hardware register*/
    board_register_hardware();

    lv_vendor_init(DISPLAY_NAME);

    ui_init();
    // lv_demo_widgets();
    // lv_demo_benchmark();

    lv_vendor_start(THREAD_PRIO_0, 1024*8);

    static THREAD_CFG_T thrd_param = {.priority = TASK_GPIO_PRIORITY, .stackDepth = TASK_GPIO_SIZE, .thrdname = "example"};
    TUYA_CALL_ERR_LOG(tal_thread_create_and_start(&sg_example_handle, NULL, NULL, __example_task, NULL, &thrd_param));

}

/**
 * @brief main
 *
 * @param argc
 * @param argv
 * @return void
 */
#if OPERATING_SYSTEM == SYSTEM_LINUX
void main(int argc, char *argv[])
{
    user_main();
    
    while (1) {
        tal_system_sleep(500);
    }
}
#else

/* Tuya thread handle */
static THREAD_HANDLE ty_app_thread = NULL;

/**
 * @brief  task thread
 *
 * @param[in] arg:Parameters when creating a task
 * @return none
 */
static void tuya_app_thread(void *arg)
{
    (void) arg;

    user_main();

    tal_thread_delete(ty_app_thread);
    ty_app_thread = NULL;
}

void tuya_app_main(void)
{
    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 4;
    thrd_param.priority = 4;
    thrd_param.thrdname = "tuya_app_main";
    tal_thread_create_and_start(&ty_app_thread, NULL, NULL, tuya_app_thread, NULL, &thrd_param);
}
#endif