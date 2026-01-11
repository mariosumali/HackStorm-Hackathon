/**
 * @file gui_display.h
 * @brief GUI display module for LVGL interface
 * @version 1.0.0
 * @copyright Copyright (c) 2025
 */

#ifndef _GUI_DISPLAY_H_
#define _GUI_DISPLAY_H_

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize GUI display
 * @return OPERATE_RET - OPRT_OK on success
 */
OPERATE_RET gui_display_init(void);

/**
 * @brief Start LVGL task
 * @return OPERATE_RET - OPRT_OK on success
 */
OPERATE_RET gui_display_start(void);

/**
 * @brief Update status label on display
 * @param status_text - text to display
 */
void gui_display_set_status(const char *status_text);

/**
 * @brief Set transcription result text
 * @param text - transcription text to display
 */
void gui_display_set_result(const char *text);

/**
 * @brief Clear transcription result
 */
void gui_display_clear_result(void);

#ifdef __cplusplus
}
#endif

#endif /* _GUI_DISPLAY_H_ */
