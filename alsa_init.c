#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>

// --- 定义 WAV 文件头结构体 ---
// 所有的 WAV 文件开头都有这 44 个字节的说明书
struct WAV_HEADER {
    char riff_id[4];      // "RIFF"
    uint32_t riff_sz;     // 文件总大小 - 8
    char riff_fmt[4];     // "WAVE"
    char fmt_id[4];       // "fmt "
    uint32_t fmt_sz;      // fmt块大小 (16)
    uint16_t audio_fmt;   // 格式 (1 = PCM)
    uint16_t num_chn;     // 通道数 (2)
    uint32_t sample_rate; // 采样率 (44100)
    uint32_t byte_rate;   // 字节率 = 采样率 * 帧大小
    uint16_t block_align; // 帧大小 (4)
    uint16_t bits_per_sample; // 位深 (16)
    char data_id[4];      // "data"
    uint32_t data_sz;     // 纯音频数据的大小
};

int main(int argc, char *argv[]) {
    int rc;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    unsigned int val = 44100;
    int dir;
    snd_pcm_uframes_t frames = 32;
    char *buffer;
    int size;
    
    // 我们录制 5 秒
    int seconds = 5; 
    
    // 打开 PCM 设备
    rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) {
        fprintf(stderr, "无法打开设备: %s\n", snd_strerror(rc));
        return 1;
    }

    // 设置参数
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, params, 2);
    snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
    rc = snd_pcm_hw_params(handle, params);

    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    size = frames * 4; // 2 channels * 16 bit(2 bytes) = 4 bytes frame
    buffer = (char *) malloc(size);

    // 计算总数据量
    // 总字节 = 秒数 * 采样率 * 帧大小
    uint32_t total_data_len = seconds * val * 4;
    long loops = total_data_len / size;

    // --- 准备 WAV 头 ---
    struct WAV_HEADER header;
    memcpy(header.riff_id, "RIFF", 4);
    header.riff_sz = total_data_len + 36; // 36 = header(44) - 8
    memcpy(header.riff_fmt, "WAVE", 4);
    memcpy(header.fmt_id, "fmt ", 4);
    header.fmt_sz = 16;
    header.audio_fmt = 1;       // PCM
    header.num_chn = 2;         // Stereo
    header.sample_rate = val;   // 44100
    header.block_align = 4;     // 2 * 16/8
    header.byte_rate = val * 4; // 44100 * 4
    header.bits_per_sample = 16;
    memcpy(header.data_id, "data", 4);
    header.data_sz = total_data_len;

    // 打开文件，这次我们叫它 .wav
    FILE *fp = fopen("output.wav", "wb");
    
    // 1. 先写入头信息
    fwrite(&header, sizeof(struct WAV_HEADER), 1, fp);

    printf("开始录音 5 秒...\n");
    
    // 2. 循环写入数据
    while (loops > 0) {
        loops--;
        rc = snd_pcm_readi(handle, buffer, frames);
        if (rc == -EPIPE) {
            fprintf(stderr, "Overrun!\n");
            snd_pcm_prepare(handle);
        } else if (rc < 0) {
            fprintf(stderr, "Error: %s\n", snd_strerror(rc));
        } else {
            fwrite(buffer, 1, size, fp);
        }
    }

    printf("录音完成！文件已保存为 output.wav\n");

    fclose(fp);
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    free(buffer);
    return 0;
}
