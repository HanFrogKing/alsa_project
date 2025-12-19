#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

// --- WAV 文件头结构体 ---
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

// 频率表 (基础音)
#define C4 261.63
#define D4 293.66
#define E4 329.63
#define F4 349.23
#define G4 392.00
#define A4 440.00
#define B4 493.88
#define C5 523.25

// 频率表 (和声 - 低八度或者三度，这里我们用低八度增加厚度)
#define C3 130.81
#define D3 146.83
#define E3 164.81
#define F3 174.61
#define G3 196.00
#define A3 220.00
#define B3 246.94

// --- ADSR 参数 (单位：秒) ---
// Attack(起音): 0.05s 瞬间达到最大音量
// Decay(衰减): 0.1s  稍微回落
// Sustain(延音): 0.8   保持在 80% 音量
// Release(释音): 0.2s  松手后声音慢慢消失
typedef struct {
    double attack;
    double decay;
    double sustain_level;
    double release;
} ADSR;

// ADSR 计算函数：根据当前时间 t 返回一个 0.0 ~ 1.0 的音量系数
double get_adsr_volume(double t, double total_duration, ADSR env) {
    double volume = 0.0;
    double sustain_time = total_duration - env.attack - env.decay - env.release;
    
    // 防止音符太短导致时间计算出错
    if (sustain_time < 0) sustain_time = 0;

    if (t < env.attack) {
        // Attack 阶段: 0 -> 1
        volume = t / env.attack;
    } else if (t < env.attack + env.decay) {
        // Decay 阶段: 1 -> Sustain
        double progress = (t - env.attack) / env.decay;
        volume = 1.0 - progress * (1.0 - env.sustain_level);
    } else if (t < total_duration - env.release) {
        // Sustain 阶段: 保持 Sustain Level
        volume = env.sustain_level;
    } else {
        // Release 阶段: Sustain -> 0
        double release_start_time = total_duration - env.release;
        double progress = (t - release_start_time) / env.release;
        volume = env.sustain_level * (1.0 - progress);
    }
    
    return (volume > 0) ? volume : 0;
}

// 生成并混合音符
// buffer: 写入目标
// freq1: 主旋律频率
// freq2: 和声频率
// duration: 持续时间
// rate: 采样率
// offset: 写入位置
int generate_poly_tone(short *buffer, double freq1, double freq2, double duration, int rate, int offset) {
    int total_samples = duration * rate;
    ADSR env = {0.05, 0.1, 0.7, 0.15}; // 定义一个舒服的包络

    for (int i = 0; i < total_samples; i++) {
        double t = (double)i / rate;
        
        // 1. 计算 ADSR 音量 (让声音有动态)
        double vol_factor = get_adsr_volume(t, duration, env);

        // 2. 生成两个波形 (主旋律 + 和声)
        // 振幅设为 8000，两个加起来 16000，不会爆音 (最大32767)
        double wave1 = 8000.0 * sin(2.0 * M_PI * freq1 * t); // 主旋律
        double wave2 = 6000.0 * sin(2.0 * M_PI * freq2 * t); // 和声 (稍微小声点)

        // 3. 混音 (直接相加) 并应用 ADSR
        short mixed_sample = (short)((wave1 + wave2) * vol_factor);
        
        // 写入立体声
        buffer[offset + i*2]     = mixed_sample; 
        buffer[offset + i*2 + 1] = mixed_sample; 
    }
    
    return offset + total_samples * 2;
}

int main() {
    FILE *fp = fopen("music_poly.wav", "wb");
    if (!fp) { perror("打开文件失败"); return 1; }

    int rate = 44100;
    int channels = 2;
    int bits = 16;
    double duration = 0.5; 
    
    // 主旋律: 1 1 5 5 6 6 5
    double melody[] = { C4, C4, G4, G4, A4, A4, G4, F4, F4, E4, E4, D4, D4, C4 };
    // 和声:   1 3 3 5 4 6 5 (简单的三度/五度配合)
    double harmony[] = { C3, E3, E3, G3, F3, A3, E3, D3, A3, G3, G3, F3, F3, E3 };

    int note_count = sizeof(melody) / sizeof(double);
    int total_samples = note_count * duration * rate;
    int data_size = total_samples * channels * (bits / 8);

    // --- 填充 WAV 头 (还是老样子) ---
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

    // --- 生成 PCM 数据 ---
    short *pcm_data = (short *)malloc(data_size);
    int offset = 0;

    for (int i = 0; i < note_count; i++) {
        // 同时把主旋律和和声传进去
        offset = generate_poly_tone(pcm_data, melody[i], harmony[i], duration, rate, offset);
    }

    fwrite(pcm_data, 1, data_size, fp);
    free(pcm_data);
    fclose(fp);
    printf("生成完毕！文件名为 music_poly.wav\n");
    return 0;
}
