/**
 * @file voice_recorder.c
 * @brief Voice recorder module implementation
 * @version 1.0.0
 * @copyright Copyright (c) 2025
 */

#include "voice_recorder.h"
#include "tuya_cloud_types.h"
#include "tuya_ringbuf.h"
#include "tal_api.h"
#include "tkl_output.h"
#include "tdl_audio_manage.h"
#include "board_com_api.h"

/***********************************************************
************************macro define************************
***********************************************************/
// Maximum recording duration in milliseconds
#define RECORDER_MAX_DURATION_MS (3 * 1000)

/***********************************************************
***********************variable define**********************
***********************************************************/
static VOICE_RECORDER_STATUS_E sg_status           = VOICE_RECORDER_IDLE;
static TDL_AUDIO_HANDLE_T      sg_audio_hdl        = NULL;
static TDL_AUDIO_INFO_T        sg_audio_info       = {0};
static TUYA_RINGBUFF_T         sg_pcm_ringbuf      = NULL;
static uint8_t                *sg_playback_buf     = NULL;
static uint32_t                sg_playback_len     = 0;
static uint32_t                sg_playback_max_len = 0;
static bool                    sg_is_playing       = false;

static VOICE_RECORDER_STATUS_CB sg_status_cb = NULL;
static VOICE_RECORDER_RESULT_CB sg_result_cb = NULL;
static VOICE_RECORDER_VIS_CB    sg_vis_cb    = NULL;

static uint32_t sg_record_start_time = 0;

/***********************************************************
***********************function define**********************
***********************************************************/
static void __notify_status_change(VOICE_RECORDER_STATUS_E new_status)
{
    sg_status = new_status;
    if (sg_status_cb) {
        sg_status_cb(sg_status);
    }
}

// Helper to dump audio as hex for PC streaming
static void __hex_dump_audio(uint8_t *data, uint32_t len)
{
    // Print in chunks to avoid buffer limits if any, though PR_NOTICE usually handles it.
    // Format: AUDIO:<hex_string>
    // We'll trust the logger to handle meaningful line lengths.
    // If len is large, we might need multiple lines.
    // Audio frames are usually small (e.g. 128 bytes - 1024 bytes).

    // Simple implementation: print chunk by chunk if needed.
    // Here we assume frame size is manageable (e.g. < 2KB).
    // Note: Logging overhead is high.

    // Using a static buffer to save stack
    // Max frame size guess: 2048 bytes -> 4096 hex chars + prefix
    // Just handle small chunks for safety.

#define CHUNK_SIZE 64 // process 64 bytes at a time (128 hex chars)
    char hex_buf[CHUNK_SIZE * 2 + 1];

    for (uint32_t i = 0; i < len; i += CHUNK_SIZE) {
        uint32_t chunk = (len - i) > CHUNK_SIZE ? CHUNK_SIZE : (len - i);
        for (uint32_t j = 0; j < chunk; j++) {
            snprintf(&hex_buf[j * 2], 3, "%02X", data[i + j]);
        }
        PR_NOTICE("AUDIO:%s", hex_buf);
    }
}

static void __audio_frame_callback(TDL_AUDIO_FRAME_FORMAT_E type, TDL_AUDIO_STATUS_E status, uint8_t *data,
                                   uint32_t len)
{
    (void)type;
    (void)status;
    if (sg_status == VOICE_RECORDER_RECORDING) {

        // 0. Stream to PC (Removed for Store-and-Forward)
        // __hex_dump_audio(data, len);

        // 1. Write to ring buffer for processing/upload mocks
        if (sg_pcm_ringbuf) {
            tuya_ring_buff_write(sg_pcm_ringbuf, data, len);
        }

        // 2. Write to linear playback buffer
        if (sg_playback_buf && sg_playback_len + len <= sg_playback_max_len) {
            memcpy(sg_playback_buf + sg_playback_len, data, len);
            sg_playback_len += len;
        }

        // 3. Trigger visualization callback
        if (sg_vis_cb) {
            sg_vis_cb((int16_t *)data, len / 2); // len is bytes, PCM is 16-bit
        }
    }
}

static void __process_transcription(void)
{
    // Mock transcription - in a real implementation, this would:
    // 1. Send audio to cloud ASR service
    // 2. Wait for response
    // 3. Return transcribed text

    // For now, we simulate processing with a delay and mock response
    __notify_status_change(VOICE_RECORDER_PROCESSING);

    tal_system_sleep(500); // Simulate processing time

    // Mock transcription result
    if (sg_result_cb) {
        uint32_t data_len = tuya_ring_buff_used_size_get(sg_pcm_ringbuf);
        char     result_text[128];
        snprintf(result_text, sizeof(result_text),
                 "[Recording captured: %d bytes of audio data]\n\n"
                 "Transcription would appear here with cloud STT integration.",
                 data_len);
        sg_result_cb(result_text);
    }

    // Reset ring buffer
    tuya_ring_buff_reset(sg_pcm_ringbuf);

    __notify_status_change(VOICE_RECORDER_DONE);

    // Go back to idle after a short delay
    tal_system_sleep(100);
    __notify_status_change(VOICE_RECORDER_IDLE);
}

OPERATE_RET voice_recorder_init(void)
{
    OPERATE_RET rt      = OPRT_OK;
    uint32_t    buf_len = 0;

    // Find and open audio codec
    TUYA_CALL_ERR_RETURN(tdl_audio_find(AUDIO_CODEC_NAME, &sg_audio_hdl));
    TUYA_CALL_ERR_RETURN(tdl_audio_open(sg_audio_hdl, __audio_frame_callback));
    TUYA_CALL_ERR_RETURN(tdl_audio_get_info(sg_audio_hdl, &sg_audio_info));

    if (0 == sg_audio_info.frame_size || 0 == sg_audio_info.sample_tm_ms) {
        PR_ERR("Invalid audio info");
        return OPRT_INVALID_PARM;
    }

    // Create ring buffer for audio data (keep this for stream processing sim)
    buf_len = (RECORDER_MAX_DURATION_MS / sg_audio_info.sample_tm_ms) * sg_audio_info.frame_size;
    TUYA_CALL_ERR_RETURN(tuya_ring_buff_create(buf_len, OVERFLOW_PSRAM_STOP_TYPE, &sg_pcm_ringbuf));

    // Allocate linear playback buffer
    sg_playback_max_len = buf_len;
    sg_playback_buf     = (uint8_t *)tal_malloc(sg_playback_max_len);
    if (NULL == sg_playback_buf) {
        PR_ERR("Malloc playback buffer failed! Playback disabled.");
        // Non-fatal error: Allow app to run without playback
        // return OPRT_MALLOC_FAILED;
    }

    // Set volume
    tdl_audio_volume_set(sg_audio_hdl, 80);

    PR_NOTICE("Voice recorder initialized successfully");
    PR_DEBUG("Sample rate: %d, bits: %d, channels: %d", sg_audio_info.sample_rate, sg_audio_info.sample_bits,
             sg_audio_info.sample_ch_num);

    return OPRT_OK;
}

OPERATE_RET voice_recorder_start(void)
{
    if (sg_status == VOICE_RECORDER_RECORDING) {
        PR_WARN("Already recording");
        return OPRT_OK;
    }

    // Reset buffers
    tuya_ring_buff_reset(sg_pcm_ringbuf);
    sg_playback_len = 0;

    sg_record_start_time = tal_system_get_millisecond();
    __notify_status_change(VOICE_RECORDER_RECORDING);

    PR_NOTICE("Recording started");
    return OPRT_OK;
}

OPERATE_RET voice_recorder_stop(void)
{
    if (sg_status != VOICE_RECORDER_RECORDING) {
        PR_WARN("Not recording");
        return OPRT_OK;
    }

    uint32_t duration = tal_system_get_millisecond() - sg_record_start_time;
    PR_NOTICE("Recording stopped, duration: %d ms", duration);

    // Process transcription
    __process_transcription();

    return OPRT_OK;
}

OPERATE_RET voice_recorder_start_play(void)
{
    if (sg_status == VOICE_RECORDER_RECORDING) {
        PR_WARN("Cannot play while recording");
        return OPRT_COM_ERROR;
    }

    if (sg_playback_len == 0) {
        PR_WARN("No audio to play");
        return OPRT_COM_ERROR;
    }

    PR_NOTICE("Starting playback, len: %d", sg_playback_len);
    tdl_audio_play(sg_audio_hdl, sg_playback_buf, sg_playback_len);
    sg_is_playing = true;

    return OPRT_OK;
}

OPERATE_RET voice_recorder_stop_play(void)
{
    if (sg_is_playing) {
        tdl_audio_play_stop(sg_audio_hdl);
        sg_is_playing = false;
        PR_NOTICE("Playback stopped");
    }
    return OPRT_OK;
}

OPERATE_RET voice_recorder_upload_dump(void)
{
    // Check if we have data
    if (sg_playback_len == 0 || sg_playback_buf == NULL) {
        PR_WARN("No data to upload");
        return OPRT_COM_ERROR;
    }

    PR_NOTICE("UPLOAD_START");

    // Allow system to breathe before dumping
    tal_system_sleep(100);

    // Reuse hex dump helper but with delay to prevent flooding if needed
    // Hex dump helper prints AUDIO: prefix
    __hex_dump_audio(sg_playback_buf, sg_playback_len);

    tal_system_sleep(100);
    PR_NOTICE("UPLOAD_END");

    return OPRT_OK;
}

VOICE_RECORDER_STATUS_E voice_recorder_get_status(void)
{
    return sg_status;
}

void voice_recorder_register_status_cb(VOICE_RECORDER_STATUS_CB cb)
{
    sg_status_cb = cb;
}

void voice_recorder_register_result_cb(VOICE_RECORDER_RESULT_CB cb)
{
    sg_result_cb = cb;
}

void voice_recorder_register_vis_cb(VOICE_RECORDER_VIS_CB cb)
{
    sg_vis_cb = cb;
}

void voice_recorder_process(void)
{
    // Check for auto-stop if recording exceeds max duration
    if (sg_status == VOICE_RECORDER_RECORDING) {
        uint32_t elapsed = tal_system_get_millisecond() - sg_record_start_time;
        if (elapsed >= RECORDER_MAX_DURATION_MS) {
            PR_NOTICE("Max recording duration reached, auto-stopping");
            voice_recorder_stop();
        }
    }
}
