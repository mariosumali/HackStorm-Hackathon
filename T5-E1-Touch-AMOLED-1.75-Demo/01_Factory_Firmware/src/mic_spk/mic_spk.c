#include "mic_spk.h"

#include "tdl_audio_manage.h"

/***********************************************************
***********************variable define**********************
***********************************************************/
bool Start_Recording = FALSE;
static THREAD_HANDLE recorder_hdl = NULL;

struct recorder_ctx sg_recorder_ctx = {
    .pcm_buf = NULL,
    .recording = FALSE,
    .playing = FALSE,
    .file_hdl = NULL,
};


static TDL_AUDIO_HANDLE_T audio_hdl = NULL;
/***********************************************************
***********************function define**********************
***********************************************************/
static void app_audio_trigger_pin_init(void)
{
    dev_gpio_init(AUDIO_TRIGGER_PIN, TUYA_GPIO_INPUT);
}

static BOOL_T audio_trigger_pin_is_pressed(void)
{
    TUYA_GPIO_LEVEL_E level = TUYA_GPIO_LEVEL_HIGH;
    dev_digital_read(AUDIO_TRIGGER_PIN, &level);
    return (level == TUYA_GPIO_LEVEL_LOW) ? TRUE : FALSE;
}

static void _audio_frame_put(TDL_AUDIO_FRAME_FORMAT_E type, TDL_AUDIO_STATUS_E status, uint8_t *data, uint32_t len)
{
    (void)type;
    (void)status;

    if (NULL == sg_recorder_ctx.pcm_buf) {
        return;
    }

    if (sg_recorder_ctx.recording) {
        tuya_ring_buff_write(sg_recorder_ctx.pcm_buf, data, len);
    }

    return;
}

static void app_audio_init(void)
{
#if 0
    int ret = 0;
    TKL_AUDIO_CONFIG_T config ={0};

    config.enable = false;
    config.card = TKL_AUDIO_TYPE_BOARD;
    config.ai_chn = TKL_AI_0;
    config.spk_gpio_polarity= 0;     // sample
    config.sample = MIC_SAMPLE_RATE;     // sample
    config.datebits = MIC_SAMPLE_BITS;   // datebit
    config.channel = TKL_AUDIO_CHANNEL_MONO;                         // channel num
    config.codectype = TKL_CODEC_AUDIO_PCM;     // codec type

    config.spk_gpio = SPEAKER_ENABLE_PIN;
    config.put_cb = _audio_frame_put;

    PR_DEBUG("TODO ... %s %d", __func__, __LINE__);

    ret = tkl_ai_init(&config, 1);

    ret |= tkl_ai_set_vol(TKL_AUDIO_TYPE_BOARD, 0, 80);

    ret |= tkl_ai_start(TKL_AUDIO_TYPE_BOARD,0);

    tkl_ao_set_vol(TKL_AUDIO_TYPE_BOARD, 0, NULL, 100);
#else
    OPERATE_RET rt = OPRT_OK;

    rt = tdl_audio_find(AUDIO_CODEC_NAME, &audio_hdl);
    if (OPRT_OK != rt) {
        PR_ERR("tdl_audio_find failed, rt = %d", rt);
        return;
    }
    rt = tdl_audio_open(audio_hdl, _audio_frame_put);
    if (OPRT_OK != rt) {
        PR_ERR("tdl_audio_open failed, rt = %d", rt);
        return;
    }
    // set volume to 80
    rt = tdl_audio_volume_set(audio_hdl, 80);
    if (OPRT_OK != rt) {
        PR_ERR("tdl_audio_volume_set failed, rt = %d", rt);
        return;
    }
#endif
    return;
}

static void app_mic_record(void)
{
#if RECORDER_FILE_SOURCE == USE_RING_BUFFER
    // Nothing to do
    // Recording through ring buffer will only record the first MIC_RECORD_DURATION_MS of data
    // It will not record subsequent data to avoid overwriting data in the ring buffer, which would corrupt the PCM
    // format and cause playback anomalies
#elif (RECORDER_FILE_SOURCE == USE_INTERNAL_FLASH) || (RECORDER_FILE_SOURCE == USE_SD_CARD)
    if (NULL == sg_recorder_ctx.file_hdl) {
        return;
    }

    uint32_t data_len = 0;
    data_len = tuya_ring_buff_used_size_get(sg_recorder_ctx.pcm_buf);
    if (data_len == 0) {
        return;
    }

    char *read_buf = tkl_system_psram_malloc(data_len);
    if (NULL == read_buf) {
        PR_ERR("tkl_system_psram_malloc failed");
        return;
    }

    // Write to file
    tuya_ring_buff_read(sg_recorder_ctx.pcm_buf, read_buf, data_len);
    int rt_len = tkl_fwrite(read_buf, data_len, sg_recorder_ctx.file_hdl);
    if (rt_len != (int)data_len) {
        PR_ERR("write file failed, maybe disk full");
        PR_ERR("write len %d, data len %d", rt_len, data_len);
    }

    if (read_buf) {
        tkl_system_psram_free(read_buf);
        read_buf = NULL;
    }
#endif
    return;
}

#if RECORDER_FILE_SOURCE == USE_RING_BUFFER
static void app_recode_play_from_ringbuf(void)
{
    if (NULL == sg_recorder_ctx.pcm_buf) {
        return;
    }

    uint32_t data_len = tuya_ring_buff_used_size_get(sg_recorder_ctx.pcm_buf);
    if (data_len == 0) {
        return;
    }

    uint32_t out_len = 0;
    char *frame_buf = tkl_system_psram_malloc(PCM_FRAME_SIZE);
    if (NULL == frame_buf) {
        PR_ERR("tkl_system_psram_malloc failed");
        return;
    }

    do {
        memset(frame_buf, 0, PCM_FRAME_SIZE);
        out_len = 0;

        data_len = tuya_ring_buff_used_size_get(sg_recorder_ctx.pcm_buf);
        if (data_len == 0) {
            break;
        }

        if (data_len > PCM_FRAME_SIZE) {
            tuya_ring_buff_read(sg_recorder_ctx.pcm_buf, frame_buf, PCM_FRAME_SIZE);
            out_len = PCM_FRAME_SIZE;
        } else {
            tuya_ring_buff_read(sg_recorder_ctx.pcm_buf, frame_buf, data_len);
            out_len = data_len;
        }

        // PR_NOTICE("data_len %d out_len %d", data_len, out_len);

        TKL_AUDIO_FRAME_INFO_T frame_info;
        frame_info.pbuf = frame_buf;
        frame_info.used_size = out_len;
        tkl_ao_put_frame(0, 0, NULL, &frame_info);
    } while (1);

    if (frame_buf) {
        tkl_system_psram_free(frame_buf);
        frame_buf = NULL;
    }
}
#endif

#if (RECORDER_FILE_SOURCE == USE_INTERNAL_FLASH) || (RECORDER_FILE_SOURCE == USE_SD_CARD)
static void app_recode_play_from_flash(void)
{
    // Read file
    TUYA_FILE file_hdl = tkl_fopen(RECORDER_FILE_PATH, "r");
    if (NULL == file_hdl) {
        PR_ERR("open file failed");
        return;
    }

    uint32_t data_len = 0;
    char *read_buf = tkl_system_psram_malloc(PCM_FRAME_SIZE);
    if (NULL == read_buf) {
        PR_ERR("tkl_system_psram_malloc failed");
        return;
    }

    do {
        memset(read_buf, 0, PCM_FRAME_SIZE);
        data_len = tkl_fread(read_buf, PCM_FRAME_SIZE, file_hdl);
        if (data_len == 0) {
            break;
        }

        TKL_AUDIO_FRAME_INFO_T frame_info;
        frame_info.pbuf = read_buf;
        frame_info.used_size = data_len;
        tkl_ao_put_frame(0, 0, NULL, &frame_info);
    } while (1);

    if (read_buf) {
        tkl_system_psram_free(read_buf);
        read_buf = NULL;
    }

    if (file_hdl) {
        tkl_fclose(file_hdl);
        file_hdl = NULL;
    }
}
#endif

static void app_recode_play(void)
{
#if RECORDER_FILE_SOURCE == USE_RING_BUFFER
    app_recode_play_from_ringbuf();
#elif (RECORDER_FILE_SOURCE == USE_INTERNAL_FLASH) || (RECORDER_FILE_SOURCE == USE_SD_CARD)
    app_recode_play_from_flash();
#endif
    return;
}

#if (RECORDER_FILE_SOURCE == USE_SD_CARD)
static OPERATE_RET app_pcm_to_wav(char *pcm_file)
{
    OPERATE_RET rt = OPRT_OK;

    uint8_t head[WAV_HEAD_LEN] = {0};
    uint32_t pcm_len = 0;
    uint32_t sample_rate = MIC_SAMPLE_RATE;
    uint16_t bit_depth = MIC_SAMPLE_BITS;
    uint16_t channel = MIC_CHANNEL;

    // Get pcm file length
    TUYA_FILE pcm_hdl = tkl_fopen(pcm_file, "r");
    if (NULL == pcm_hdl) {
        PR_ERR("open file failed");
        return OPRT_FILE_OPEN_FAILED;
    }
    tkl_fseek(pcm_hdl, 0, 2);
    pcm_len = tkl_ftell(pcm_hdl);

    tkl_fclose(pcm_hdl);
    pcm_hdl = NULL;

    PR_DEBUG("pcm file len %d", pcm_len);
    if (pcm_len == 0) {
        PR_ERR("pcm file is empty");
        return OPRT_COM_ERROR;
    }

    // Get wav head
    rt = app_get_wav_head(pcm_len, 1, sample_rate, bit_depth, channel, head);
    if (OPRT_OK != rt) {
        PR_ERR("app_get_wav_head failed, rt = %d", rt);
        return rt;
    }

    PR_HEXDUMP_DEBUG("wav head", head, WAV_HEAD_LEN);

    // Create wav file
    TUYA_FILE wav_hdl = tkl_fopen(RECORDER_WAV_FILE_PATH, "w");
    if (NULL == wav_hdl) {
        PR_ERR("open file: %s failed", RECORDER_WAV_FILE_PATH);
        rt = OPRT_FILE_OPEN_FAILED;
        goto __EXIT;
    }

    // Write wav head
    tkl_fwrite(head, WAV_HEAD_LEN, wav_hdl);

    // Read pcm file

    char *read_buf = NULL;
    read_buf = tkl_system_psram_malloc(PCM_FRAME_SIZE);
    if (NULL == read_buf) {
        PR_ERR("tkl_system_psram_malloc failed");
        // return OPRT_COM_ERROR;
        rt = OPRT_MALLOC_FAILED;
        goto __EXIT;
    }

    PR_DEBUG("pcm file len %d", pcm_len);
    pcm_hdl = tkl_fopen(pcm_file, "r");
    if (NULL == pcm_hdl) {
        PR_ERR("open file failed");
        rt = OPRT_FILE_OPEN_FAILED;
        goto __EXIT;
    }

    tkl_fseek(pcm_hdl, WAV_HEAD_LEN, 0);

    // Write wav data
    do {
        memset(read_buf, 0, PCM_FRAME_SIZE);
        uint32_t read_len = tkl_fread(read_buf, PCM_FRAME_SIZE, pcm_hdl);
        if (read_len == 0) {
            break;
        }

        tkl_fwrite(read_buf, read_len, wav_hdl);
    } while (1);

__EXIT:
    if (pcm_hdl) {
        tkl_fclose(pcm_hdl);
        pcm_hdl = NULL;
    }

    if (wav_hdl) {
        tkl_fclose(wav_hdl);
        wav_hdl = NULL;
    }

    if (read_buf) {
        tkl_system_psram_free(read_buf);
        read_buf = NULL;
    }

    return rt;
}
#endif

static void app_recorder_thread(void *arg)
{
    (void)arg;
    app_audio_trigger_pin_init();
    app_audio_init();

    for (;;) {
        app_mic_record();

        if ((FALSE == audio_trigger_pin_is_pressed()) && (FALSE == Start_Recording)) {
            tal_system_sleep(100);

            // End recording
            if (TRUE == sg_recorder_ctx.recording) {
                sg_recorder_ctx.recording = FALSE;
                sg_recorder_ctx.playing = TRUE;
#if (RECORDER_FILE_SOURCE == USE_INTERNAL_FLASH) || (RECORDER_FILE_SOURCE == USE_SD_CARD)
                if (sg_recorder_ctx.file_hdl) {
                    tkl_fclose(sg_recorder_ctx.file_hdl);
                    sg_recorder_ctx.file_hdl = NULL;
                }
                // Convert pcm to wav
#if (RECORDER_FILE_SOURCE == USE_SD_CARD)
                OPERATE_RET rt = OPRT_OK;    
                rt = app_pcm_to_wav(RECORDER_FILE_PATH);
                if (OPRT_OK != rt) {
                    PR_ERR("app_pcm_to_wav failed, rt = %d", rt);
                }
#endif
#endif
                PR_DEBUG("stop recording");
            }

            // Start playing
            if (TRUE == sg_recorder_ctx.playing) {
                PR_DEBUG("start playing");
                sg_recorder_ctx.playing = FALSE;
                app_recode_play();
                PR_DEBUG("stop playing");
            }

            continue;
        }

        // Start recording
        if (FALSE == sg_recorder_ctx.recording) {
#if (RECORDER_FILE_SOURCE == USE_INTERNAL_FLASH) || (RECORDER_FILE_SOURCE == USE_SD_CARD)
            // If recording file exists, delete it
            BOOL_T is_exist = FALSE;
            tkl_fs_is_exist(RECORDER_FILE_PATH, &is_exist);
            if (is_exist == TRUE) {
                tkl_fs_remove(RECORDER_FILE_PATH);
                PR_DEBUG("remove file %s", RECORDER_FILE_PATH);
            }

            is_exist = FALSE;
            tkl_fs_is_exist(RECORDER_WAV_FILE_PATH, &is_exist);
            if (is_exist == TRUE) {
                tkl_fs_remove(RECORDER_WAV_FILE_PATH);
                PR_DEBUG("remove file %s", RECORDER_WAV_FILE_PATH);
            }

            // Create recording file
            sg_recorder_ctx.file_hdl = tkl_fopen(RECORDER_FILE_PATH, "w");
            if (NULL == sg_recorder_ctx.file_hdl) {
                PR_ERR("open file failed");
                continue;
            }
            PR_DEBUG("open file %s success", RECORDER_FILE_PATH);
#endif
            tuya_ring_buff_reset(sg_recorder_ctx.pcm_buf);
            sg_recorder_ctx.recording = TRUE;
            sg_recorder_ctx.playing = FALSE;
            PR_DEBUG("start recording");
        }

        tal_system_sleep(20);
    }
}

static OPERATE_RET app_fs_init(void)
{
    OPERATE_RET rt = OPRT_OK;

#if RECORDER_FILE_SOURCE == USE_INTERNAL_FLASH
    rt = tkl_fs_mount("/", DEV_INNER_FLASH);
    if (rt != OPRT_OK) {
        PR_ERR("mount fs failed ");
        return rt;
    }
    PR_DEBUG("mount inner flash success ");
#elif RECORDER_FILE_SOURCE == USE_SD_CARD
    // rt = tkl_fs_mount("/sdcard", DEV_SDCARD);
    // if (rt != OPRT_OK) {
    //     PR_ERR("mount sd card failed, please retry after format ");
    //     return rt;
    // }
    // PR_DEBUG("mount sd card success ");
#else
    return rt;
#endif

    return rt;
}

void mic_spk_test(void)
{
    OPERATE_RET rt = OPRT_OK;

    // Initialize file system
    rt = app_fs_init();
    if (OPRT_OK != rt) {
        PR_ERR("app_fs_init failed, rt = %d", rt);
        return;
    }

    // Create PCM buffer
    if (NULL == sg_recorder_ctx.pcm_buf) {
        PR_DEBUG("create pcm buffer size %d", PCM_BUF_SIZE);
        rt = tuya_ring_buff_create(PCM_BUF_SIZE, OVERFLOW_PSRAM_STOP_TYPE, &sg_recorder_ctx.pcm_buf);
        if (OPRT_OK != rt) {
            PR_ERR("tuya_ring_buff_create failed, rt = %d", rt);
            return;
        }
    }

    THREAD_CFG_T thrd_param = {0};
    thrd_param.stackDepth = 1024 * 6;
    thrd_param.priority = THREAD_PRIO_3;
    thrd_param.thrdname = "recorder task";
    tal_thread_create_and_start(&recorder_hdl, NULL, NULL, app_recorder_thread, NULL, &thrd_param);
    return;
}

