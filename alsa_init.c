#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>

int main(int argc, char *argv[]) {
    int rc;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params; 
    unsigned int val = 44100;    
    int dir;
    snd_pcm_uframes_t frames = 32;

    printf("ALSA library version: %s\n", SND_LIB_VERSION_STR);

    // 1. 打开 PCM 设备 (录音模式)
    rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) {
        fprintf(stderr, "无法打开PCM设备: %s\n", snd_strerror(rc));
        return 1;
    }

    // --- 硬件参数配置 ---
    snd_pcm_hw_params_alloca(&params); // 分配内存
    snd_pcm_hw_params_any(handle, params); // 初始化
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED); // 交错模式
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE); // 16位小端
    snd_pcm_hw_params_set_channels(handle, params, 2); // 双声道
    snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir); // 44100Hz

    // 写入参数
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) {
        fprintf(stderr, "无法设置硬件参数: %s\n", snd_strerror(rc));
        return 1;
    }

    printf("参数配置成功！\n");
    printf("采样率: %d Hz\n", val);

    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    printf("周期大小: %lu 帧\n", frames);

    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    return 0;
}
