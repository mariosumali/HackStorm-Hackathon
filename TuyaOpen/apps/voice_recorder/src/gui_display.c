/**
 * @file gui_display.c
 * @brief LVGL GUI display module implementation
 * @version 1.0.0
 * @copyright Copyright (c) 2025
 */

#include "gui_display.h"
#include "voice_recorder.h"
#include "tuya_cloud_types.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "lvgl.h"
#include "lv_vendor.h"
#include "simple_fft.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 480

/***********************************************************
***********************variable define**********************
***********************************************************/
static lv_obj_t          *sg_status_label    = NULL;
static lv_obj_t          *sg_result_textarea = NULL;
static lv_obj_t          *sg_record_btn      = NULL;
static lv_obj_t          *sg_btn_label       = NULL;
static lv_obj_t          *sg_play_btn        = NULL;
static lv_obj_t          *sg_upload_btn      = NULL;
static lv_obj_t          *sg_play_label      = NULL;
static lv_obj_t          *sg_chart           = NULL;
static lv_chart_series_t *sg_ser1            = NULL;

static bool sg_is_recording = false;
static bool sg_is_playing   = false;

/***********************************************************
***********************function define**********************
***********************************************************/
static void __record_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        if (!sg_is_recording) {
            // Start recording
            voice_recorder_start();
            sg_is_recording = true;
            lv_label_set_text(sg_btn_label, "STOP");
            lv_obj_set_style_bg_color(sg_record_btn, lv_color_hex(0xFF4444), 0);

            // Hide Play button while recording
            if (sg_play_btn)
                lv_obj_add_flag(sg_play_btn, LV_OBJ_FLAG_HIDDEN);

            // Clear result
            if (sg_result_textarea)
                lv_textarea_set_text(sg_result_textarea, "");
        } else {
            // Stop recording
            voice_recorder_stop();
            sg_is_recording = false;
            lv_label_set_text(sg_btn_label, "RECORD");
            lv_obj_set_style_bg_color(sg_record_btn, lv_color_hex(0x4CAF50), 0);

            // Show Play button
            if (sg_play_btn)
                lv_obj_clear_flag(sg_play_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void __play_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code      = lv_event_get_code(e);
    lv_obj_t       *opt_label = lv_obj_get_child(lv_event_get_target(e), 0);

    if (code == LV_EVENT_CLICKED) {
        if (0 == strcmp(lv_label_get_text(opt_label), "PLAY")) {
            if (voice_recorder_start_play() == OPRT_OK) {
                lv_label_set_text(opt_label, "STOP PLAY");
                // Disable record btn
                if (sg_record_btn)
                    lv_obj_add_state(sg_record_btn, LV_STATE_DISABLED);
                // Disable upload btn
                if (sg_upload_btn)
                    lv_obj_add_state(sg_upload_btn, LV_STATE_DISABLED);
            }
        } else {
            voice_recorder_stop_play();
            sg_is_playing = false;
            lv_label_set_text(sg_play_label, "PLAY");
            // Enable record btn
            if (sg_record_btn)
                lv_obj_clear_state(sg_record_btn, LV_STATE_DISABLED);
        }
    }
}

static void __upload_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        PR_NOTICE("Upload requested");
        voice_recorder_upload_dump();
    }
}

static void __status_change_callback(VOICE_RECORDER_STATUS_E status)
{
    const char *status_text = "Unknown";

    switch (status) {
    case VOICE_RECORDER_IDLE:
        status_text     = "Ready to Record";
        sg_is_recording = false;
        if (sg_btn_label) {
            lv_label_set_text(sg_btn_label, "RECORD");
        }
        if (sg_record_btn) {
            lv_obj_set_style_bg_color(sg_record_btn, lv_color_hex(0x4CAF50), 0);
        }
        break;
    case VOICE_RECORDER_RECORDING:
        status_text = "Recording...";
        break;
    case VOICE_RECORDER_PROCESSING:
        status_text = "Processing...";
        break;
    case VOICE_RECORDER_DONE:
        status_text = "Complete";
        break;
    case VOICE_RECORDER_ERROR:
        status_text = "Error!";
        break;
    }

    if (sg_status_label) {
        lv_label_set_text(sg_status_label, status_text);
    }
}

static void __result_callback(const char *text)
{
    if (sg_result_textarea && text) {
        lv_textarea_set_text(sg_result_textarea, text);
    }
}

// Shared visualization buffer
static int16_t       sg_vis_buff[128];
static volatile bool sg_vis_ready = false;

static void __vis_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!sg_vis_ready || !sg_chart || !sg_ser1)
        return;

    // Process data in GUI thread
    complex_t input[128];
    int16_t   output[64];

    // Copy to local to release lock/flag quickly (though we just clear flag here)
    for (int i = 0; i < 128; i++) {
        input[i].r = (float)sg_vis_buff[i];
        input[i].i = 0;
    }
    sg_vis_ready = false; // logic: we consumed it

    // FFT
    simple_fft(input, 128);
    simple_fft_magnitude(input, output, 128);

    // Update Chart
    for (int i = 0; i < 50; i++) {
        lv_chart_set_value_by_id(sg_chart, sg_ser1, i, output[i + 1]);
    }
    lv_chart_refresh(sg_chart);
}

static void __vis_callback(int16_t *pcm_data, uint32_t len)
{
    // Called from Audio Thread - copy data only
    if (len < 128)
        return;

    // Simple lock-free: drop frame if previous not consumed
    if (!sg_vis_ready) {
        memcpy(sg_vis_buff, pcm_data, 128 * sizeof(int16_t));
        sg_vis_ready = true;
    }
}

// Declare Tuya font
LV_FONT_DECLARE(font_puhui_18_2);

static void __create_ui(void)
{
    // Get active screen
    lv_obj_t *scr = lv_scr_act();

    // Set background color
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1E1E2E), 0);

    // Title label
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Voice Recorder");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &font_puhui_18_2, 0); // Use puhui font
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    // Status label
    sg_status_label = lv_label_create(scr);
    lv_label_set_text(sg_status_label, "Ready to Record");
    lv_obj_set_style_text_color(sg_status_label, lv_color_hex(0xCDD6F4), 0);
    lv_obj_set_style_text_font(sg_status_label, &font_puhui_18_2, 0); // Use puhui font
    lv_obj_align(sg_status_label, LV_ALIGN_TOP_MID, 0, 55);

    // Record button
    sg_record_btn = lv_btn_create(scr);
    lv_obj_set_size(sg_record_btn, 140, 60);
    lv_obj_align(sg_record_btn, LV_ALIGN_TOP_MID, 0, 90);
    lv_obj_set_style_bg_color(sg_record_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_radius(sg_record_btn, 30, 0);
    lv_obj_add_event_cb(sg_record_btn, __record_btn_event_cb, LV_EVENT_CLICKED, NULL);

    sg_btn_label = lv_label_create(sg_record_btn);
    lv_label_set_text(sg_btn_label, "RECORD");
    lv_obj_set_style_text_color(sg_btn_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(sg_btn_label, &font_puhui_18_2, 0); // Use puhui font
    lv_obj_center(sg_btn_label);

    // Result text area (for transcription)
    sg_result_textarea = lv_textarea_create(scr);
    lv_obj_set_size(sg_result_textarea, SCREEN_WIDTH - 20, 80);
    lv_obj_align(sg_result_textarea, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_textarea_set_placeholder_text(sg_result_textarea, "Transcription...");
    lv_obj_set_style_bg_color(sg_result_textarea, lv_color_hex(0x313244), 0);
    lv_obj_set_style_text_color(sg_result_textarea, lv_color_hex(0xCDD6F4), 0);

    // Play Button
    sg_play_btn = lv_btn_create(scr);
    lv_obj_set_size(sg_play_btn, 100, 40);
    lv_obj_align(sg_play_btn, LV_ALIGN_BOTTOM_MID, 0, -100);
    lv_obj_set_style_bg_color(sg_play_btn, lv_color_hex(0x89B4FA), 0);
    lv_obj_add_event_cb(sg_play_btn, __play_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(sg_play_btn, LV_OBJ_FLAG_HIDDEN); // Hidden initially

    sg_play_label = lv_label_create(sg_play_btn);
    lv_label_set_text(sg_play_label, "PLAY");
    lv_obj_center(sg_play_label);

    // Upload Button
    sg_upload_btn = lv_btn_create(lv_scr_act());
    lv_obj_set_size(sg_upload_btn, 100, 50);
    lv_obj_align(sg_upload_btn, LV_ALIGN_BOTTOM_MID, 60, -50); // Shift Right
    lv_obj_add_event_cb(sg_upload_btn, __upload_btn_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_style_bg_color(sg_upload_btn, lv_color_hex(0x2196F3), 0);

    lv_obj_t *upload_label = lv_label_create(sg_upload_btn);
    lv_label_set_text(upload_label, "UPLOAD");
    lv_obj_center(upload_label);

    // Adjust Play Button alignment to be Left
    lv_obj_align(sg_play_btn, LV_ALIGN_BOTTOM_MID, -60, -50); // Shift Left

    // Chart
    sg_chart = lv_chart_create(scr);
    lv_obj_set_size(sg_chart, SCREEN_WIDTH - 20, 100);
    lv_obj_align(sg_chart, LV_ALIGN_TOP_MID, 0, 160);
    lv_chart_set_type(sg_chart, LV_CHART_TYPE_BAR);
    lv_chart_set_range(sg_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 2000); // Adjust scaling
    lv_chart_set_point_count(sg_chart, 50);
    lv_chart_set_div_line_count(sg_chart, 3, 0);
    lv_obj_set_style_bg_color(sg_chart, lv_color_hex(0x1E1E2E), 0);
    lv_obj_set_style_border_width(sg_chart, 0, 0);

    sg_ser1 = lv_chart_add_series(sg_chart, lv_color_hex(0xF38BA8), LV_CHART_AXIS_PRIMARY_Y);

    // Register callbacks with voice recorder
    voice_recorder_register_status_cb(__status_change_callback);
    voice_recorder_register_result_cb(__result_callback);
    voice_recorder_register_vis_cb(__vis_callback);

    // Create timer for visualization update (50ms)
    lv_timer_create(__vis_timer_cb, 50, NULL);

    PR_NOTICE("GUI created successfully");
}

OPERATE_RET gui_display_init(void)
{
    // Initialize LVGL vendor layer
    lv_vendor_init("display"); // Using "display" as defined in Kconfig

    // Create UI elements
    __create_ui();

    PR_NOTICE("GUI display initialized");
    return OPRT_OK;
}

OPERATE_RET gui_display_start(void)
{
    // Start LVGL task with priority 4 and stack size 4096 (or 8192 if needed)
    lv_vendor_start(THREAD_PRIO_4, 8192);

    PR_NOTICE("GUI display started");
    return OPRT_OK;
}

void gui_display_set_status(const char *status_text)
{
    if (sg_status_label && status_text) {
        lv_label_set_text(sg_status_label, status_text);
    }
}

void gui_display_set_result(const char *text)
{
    if (sg_result_textarea && text) {
        lv_textarea_set_text(sg_result_textarea, text);
    }
}

void gui_display_clear_result(void)
{
    if (sg_result_textarea) {
        lv_textarea_set_text(sg_result_textarea, "");
    }
}
