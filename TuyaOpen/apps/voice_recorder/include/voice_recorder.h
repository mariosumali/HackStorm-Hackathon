/**
 * @file voice_recorder.h
 * @brief Voice recorder module for audio capture
 * @version 1.0.0
 * @copyright Copyright (c) 2025
 */

#ifndef _VOICE_RECORDER_H_
#define _VOICE_RECORDER_H_

#include "tuya_cloud_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Recording status enumeration
 */
typedef enum {
    VOICE_RECORDER_IDLE = 0,
    VOICE_RECORDER_RECORDING,
    VOICE_RECORDER_PROCESSING,
    VOICE_RECORDER_DONE,
    VOICE_RECORDER_ERROR
} VOICE_RECORDER_STATUS_E;

/**
 * @brief Callback for recording status changes
 */
typedef void (*VOICE_RECORDER_STATUS_CB)(VOICE_RECORDER_STATUS_E status);
typedef void (*VOICE_RECORDER_RESULT_CB)(const char *text);

/**
 * @brief Initialize voice recorder
 * @return OPERATE_RET
 */
OPERATE_RET voice_recorder_init(void);

// Visualization callback
typedef void (*VOICE_RECORDER_VIS_CB)(int16_t *pcm_data, uint32_t len);

/**
 * @brief Start recording
 * @return OPERATE_RET
 */
OPERATE_RET voice_recorder_start(void);

/**
 * @brief Stop recording
 * @return OPERATE_RET
 */
OPERATE_RET voice_recorder_stop(void);

/**
 * @brief Start playback
 * @return OPERATE_RET
 */
OPERATE_RET voice_recorder_start_play(void);

/**
 * @brief Stop playback
 * @return OPERATE_RET
 */
OPERATE_RET voice_recorder_stop_play(void);

/**
 * @brief Upload recorded data to PC via UART
 * @return OPRT_OK on success
 */
OPERATE_RET voice_recorder_upload_dump(void);

/**
 * @brief Get current recording status
 * @return VOICE_RECORDER_STATUS_E
 */
VOICE_RECORDER_STATUS_E voice_recorder_get_status(void);

/**
 * @brief Register status callback
 * @param cb Callback function
 */
void voice_recorder_register_status_cb(VOICE_RECORDER_STATUS_CB cb);

/**
 * @brief Register result callback
 * @param cb Callback function
 */
void voice_recorder_register_result_cb(VOICE_RECORDER_RESULT_CB cb);

/**
 * @brief Register visualization callback
 * @param cb Callback function
 */
void voice_recorder_register_vis_cb(VOICE_RECORDER_VIS_CB cb);

/**
 * @brief Process recorded audio (call in main loop)
 */
void voice_recorder_process(void);

#ifdef __cplusplus
}
#endif

#endif /* _VOICE_RECORDER_H_ */
