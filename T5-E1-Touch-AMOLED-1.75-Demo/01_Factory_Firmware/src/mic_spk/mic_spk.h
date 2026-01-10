

#ifndef _MIC_SPK_H
#define _MIC_SPK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dev_config.h"

#include "tuya_cloud_types.h"
#include "tuya_ringbuf.h"

#include "tkl_fs.h"
#include "tkl_memory.h"
#include "tkl_audio.h"

#include "wav_encode.h"

/***********************************************************
************************macro define************************
***********************************************************/
#define USE_RING_BUFFER      0
#define USE_INTERNAL_FLASH   1 // Store recording file in internal flash
#define USE_SD_CARD          2
#define RECORDER_FILE_SOURCE USE_SD_CARD

#if (RECORDER_FILE_SOURCE == USE_INTERNAL_FLASH)
#define RECORDER_FILE_DIR      "/"
#define RECORDER_FILE_PATH     "/tuya_recorder.pcm"
#define RECORDER_WAV_FILE_PATH "/tuya_recorder.wav"
#elif (RECORDER_FILE_SOURCE == USE_SD_CARD)
#define RECORDER_FILE_DIR      "/sdcard"
#define RECORDER_FILE_PATH     "/sdcard/tuya_recorder.pcm"
#define RECORDER_WAV_FILE_PATH "/sdcard/tuya_recorder.wav"
#endif

#define SPEAKER_ENABLE_PIN TUYA_GPIO_NUM_28
#define AUDIO_TRIGGER_PIN  TUYA_GPIO_NUM_12

// MIC sample rate
#define MIC_SAMPLE_RATE TKL_AUDIO_SAMPLE_16K
// MIC sample bits
#define MIC_SAMPLE_BITS TKL_AUDIO_DATABITS_16
// MIC channel
#define MIC_CHANNEL TKL_AUDIO_CHANNEL_MONO
// Maximum recordable duration, unit ms
#define MIC_RECORD_DURATION_MS (3 * 1000)
// RINGBUF size
#define PCM_BUF_SIZE (MIC_RECORD_DURATION_MS * MIC_SAMPLE_RATE * MIC_SAMPLE_BITS * MIC_CHANNEL / 8 / 1000)
// 10ms PCM data size
#define PCM_FRAME_SIZE (10 * MIC_SAMPLE_RATE * MIC_SAMPLE_BITS * MIC_CHANNEL / 8 / 1000)

/***********************************************************
***********************typedef define***********************
***********************************************************/
struct recorder_ctx {
    TUYA_RINGBUFF_T pcm_buf;

    // Recording status
    BOOL_T recording;

    // Playing status
    BOOL_T playing;

    // Recording file handle
    TUYA_FILE file_hdl;
};

extern bool Start_Recording;
void mic_spk_test(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
