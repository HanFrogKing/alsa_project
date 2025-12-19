#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

// --- WAV 文件头结构体 (和你之前的一样) ---
struct WAV_HEADER {
    char riff_id[4];      // "RIFF"
    uint32_t riff_sz;
    char riff_fmt[4];     // "WAVE"
    char fmt_id[4];       // "fmt "
    uint32_t fmt_sz;      // 16
    uint16_t audio_fmt;   // 1 (PCM)
    uint16_t num_chn;     // 2
    uint32_t sample_rate; // 44100
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample; // 16
    char data_id[4];      // "data"
    uint32_t data_sz;
};

// 频率表 (C4 ~ B4)
#define C4 261.63
#define D4 293.66
#define E4 329.63
#define F4 349.23
#define G4 392.00
#define A4 440.00
#define B4 493.88
#define C5 523.25

// 生成一个音符的波形数据
// buffer: 写入的目标
// freq: 频率 (Hz)
// duration: 持续时间 (秒)
// rate: 采样率
// offset: 当前写到缓冲区的哪里了 (返回新的 offset)
int generate_tone(short *buffer, double freq, double duration, int rate, int offset) {
    int total_samples = duration * rate;
    
    for (int i = 0; i < total_samples; i++) {
        double t = (double)i / rate;
        
        // 核心：生成正弦波
        // 振幅设为 10000 (最大32767)，防止太吵
        short sample_value = (short)(10000 * sin(2.0 * M_PI * freq * t));
        
        // 写入立体声 (左声道 = 右声道)
        buffer[offset + i*2]     = sample_value; // Left
        buffer[offset + i*2 + 1] = sample_value; // Right
    }
    
    return offset + total_samples * 2; // 返回写完后的新位置
}

int main() {
    FILE *fp = fopen("music.wav", "wb");
    if (!fp) { perror("打开文件失败"); return 1; }

    int rate = 44100;
    int channels = 2;
    int bits = 16;
    double duration_per_note = 0.5; // 每个音符 0.5 秒
    
    // 我们要生成的旋律：1 1 5 5 6 6 5 (Twinkle Twinkle Little Star)
    double melody[] = { C4, C4, G4, G4, A4, A4, G4, F4, F4, E4, E4, D4, D4, C4 };
    int note_count = sizeof(melody) / sizeof(double);
    
    // 计算总数据量
    int total_samples = note_count * duration_per_note * rate;
    int data_size = total_samples * channels * (bits / 8);

    // --- 1. 填充 WAV 头 ---
    struct WAV_HEADER header;
    memcpy(header.riff_id, "RIFF", 4);
    header.riff_sz = 36 + data_size;
    memcpy(header.riff_fmt, "WAVE", 4);
    memcpy(header.fmt_id, "fmt ", 4);
    header.fmt_sz = 16;
    header.audio_fmt = 1;
    header.num_chn = channels;
    header.sample_rate = rate;
    header.byte_rate = rate * channels * (bits / 8);
    header.block_align = channels * (bits / 8);
    header.bits_per_sample = bits;
    memcpy(header.data_id, "data", 4);
    header.data_sz = data_size;

    fwrite(&header, sizeof(header), 1, fp);

    // --- 2. 生成 PCM 数据 ---
    short *pcm_data = (short *)malloc(data_size);
    int offset = 0;

    for (int i = 0; i < note_count; i++) {
        offset = generate_tone(pcm_data, melody[i], duration_per_note, rate, offset);
    }

    fwrite(pcm_data, 1, data_size, fp);

    // --- 3. 清理 ---
    free(pcm_data);
    fclose(fp);
    printf("生成完毕！文件名为 music.wav\n");
    return 0;
}
