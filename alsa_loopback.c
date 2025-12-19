#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>

#define FRAMES 1024

int set_params(snd_pcm_t *handle, int rate) {
    snd_pcm_hw_params_t *params;
    int rc;
    int dir;
    unsigned int val = rate;

    // 1. 分配参数对象
    snd_pcm_hw_params_alloca(&params);
    // 2. 初始化
    snd_pcm_hw_params_any(handle, params);
    // 3. 设置交错模式
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    // 4. 设置格式 (16位)
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    // 5. 设置双声道
    snd_pcm_hw_params_set_channels(handle, params, 2);
    // 6. 设置采样率 
    snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
    
    // 7. 写入硬件
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) {
        fprintf(stderr, "无法设置硬件参数: %s\n", snd_strerror(rc));
        return -1;
    }
    return 0;
}

int main() {
    int rc;
    char *buffer;
    int size;
    snd_pcm_t *capture_handle;
    snd_pcm_t *playback_handle;

    printf("一定要插耳机！否则会啸叫！\n");

    // --- 1. 打开录音设备 ---
    rc = snd_pcm_open(&capture_handle, "default", SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) {
        fprintf(stderr, "无法打开录音设备: %s\n", snd_strerror(rc));
        return 1;
    }

    // --- 2. 打开播放设备 ---
    rc = snd_pcm_open(&playback_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        fprintf(stderr, "无法打开播放设备: %s\n", snd_strerror(rc));
        return 1;
    }

    // --- 3. 配置参数 (重点看这里) ---
    // 咱们先设置成一模一样的 44100
    set_params(capture_handle, 44100);
    set_params(playback_handle, 44100);

    // --- 5. 准备缓冲区 ---
    size = FRAMES * 4; // 2声道 * 16位(2字节) = 4字节/帧
    buffer = (char *) malloc(size);

    printf("开始回声测试 (按 Ctrl+C 停止)...\n");

    // --- 5. 搬运循环 ---
    while (1) {
        // 从麦克风读
        rc = snd_pcm_readi(capture_handle, buffer, FRAMES);
        if (rc == -EPIPE) {
            fprintf(stderr, "Overrun!\n");
            snd_pcm_prepare(capture_handle);
        } else if (rc < 0) {
            fprintf(stderr, "Read Error\n");
        }

        // 往耳机写
        // 注意：这里我们直接把读到的 buffer 原封不动写出去
        rc = snd_pcm_writei(playback_handle, buffer, FRAMES);
        if (rc == -EPIPE) {
            fprintf(stderr, "Underrun!\n");
            snd_pcm_prepare(playback_handle);
        } else if (rc < 0) {
            fprintf(stderr, "Write Error\n");
        }
    }

    snd_pcm_close(capture_handle);
    snd_pcm_close(playback_handle);
    free(buffer);
    return 0;
}
