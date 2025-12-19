#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <alsa/asoundlib.h>
#include <ncurses.h>
#include <pthread.h>
#include <fftw3.h> // 引入 FFT 神器

#define FRAMES 256  // 增大缓冲区，FFT 需要足够的数据样本才能算得准
#define BARS 40     // 我们要在屏幕上画多少根柱子

// --- 全局变量 ---
volatile int keep_running = 1;
volatile int is_paused = 0;
// 这是一个共享数组，音频线程算好高度填进去，UI线程读出来画图
double spectrum_heights[BARS]; 

// --- 辅助函数：计算频谱 ---
// 这是整个程序的灵魂！
void compute_spectrum(short *buffer, int frames) {
    // 1. 准备输入数据 (FFT 需要 double 类型)
    double *in;
    fftw_complex *out;
    fftw_plan p;

    in = (double*) fftw_malloc(sizeof(double) * frames);
    out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * frames);

    // 把 short (PCM) 转成 double，并加一个简单的汉宁窗(Hanning Window)让数据更平滑
    for (int i = 0; i < frames; i++) {
        double multiplier = 0.5 * (1 - cos(2*M_PI*i/(frames-1)));
        in[i] = buffer[i] * multiplier; 
    }

    // 2. 制定 FFT 计划并执行 (Real to Complex)
    p = fftw_plan_dft_r2c_1d(frames, in, out, FFTW_ESTIMATE);
    fftw_execute(p);

    // 3. 计算这一帧的能量 (算出柱子高度)
    // FFT 的结果是对称的，我们只需要前半部分
    // 我们把结果简单的“分桶”到 BARS 根柱子里
    int samples_per_bar = (frames / 2) / BARS; 

    for (int i = 0; i < BARS; i++) {
        double power = 0;
        for (int j = 0; j < samples_per_bar; j++) {
            // index 对应的频率数据
            int index = i * samples_per_bar + j;
            // 模长 = sqrt(实部^2 + 虚部^2)
            double mag = sqrt(out[index][0]*out[index][0] + out[index][1]*out[index][1]);
            power += mag;
        }
        
        // 取平均并做一点数学缩小，防止柱子冲出屏幕
        power /= samples_per_bar;
        spectrum_heights[i] = power / 100000.0; // 这个除数取决于音量大小，可调
    }

    // 4. 清理内存 (极其重要，否则瞬间内存泄露爆炸)
    fftw_destroy_plan(p);
    fftw_free(in);
    fftw_free(out);
}

// --- 音频线程 ---
void *audio_thread_func(void *arg) {
    int rc;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    unsigned int val = 44100;
    int dir;
    snd_pcm_uframes_t frames = FRAMES;
    char *buffer;
    int size;

    // 打开文件 (确保你有 output.wav)
    FILE *fp = fopen("output.wav", "rb");
    if (!fp) return NULL;
    fseek(fp, 44, SEEK_SET);

    rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) return NULL;

    snd_pcm_hw_params_t *hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    snd_pcm_hw_params_any(handle, hw_params);
    snd_pcm_hw_params_set_access(handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, hw_params, 2);
    snd_pcm_hw_params_set_rate_near(handle, hw_params, &val, &dir);
    // 这里强制设置 buffer size，方便 FFT 计算
    snd_pcm_hw_params_set_period_size_near(handle, hw_params, &frames, &dir); 
    snd_pcm_hw_params(handle, hw_params);

    size = frames * 4; // 2 channel * 16bit
    buffer = (char *) malloc(size);

    while (keep_running) {
        if (is_paused) { usleep(100000); continue; }

        if (fread(buffer, 1, size, fp) == 0) {
            fseek(fp, 44, SEEK_SET);
            continue;
        }

        // >>> 在播放之前，先算频谱！ <<<
        // 我们只取左声道数据来分析 (short 是间隔排列的 L R L R)
        // 创建一个临时 buffer 存左声道
        short pcm_data[FRAMES];
        short *raw_data = (short*)buffer;
        for(int i=0; i<FRAMES; i++) {
            pcm_data[i] = raw_data[i*2]; // 取偶数位，即左声道
        }
        compute_spectrum(pcm_data, FRAMES); // 计算！

        rc = snd_pcm_writei(handle, buffer, frames);
        if (rc == -EPIPE) snd_pcm_prepare(handle);
    }

    fclose(fp);
    snd_pcm_close(handle);
    free(buffer);
    return NULL;
}

// --- UI 线程 ---
int main() {
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, audio_thread_func, NULL);

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    timeout(50); // 刷新率提高一点，让动画更流畅

    // 启用颜色 (让柱子变帅)
    start_color();
    init_pair(1, COLOR_CYAN, COLOR_BLACK); // 青色柱子
    init_pair(2, COLOR_GREEN, COLOR_BLACK); // 绿色文字

    while (keep_running) {
        erase(); // 清屏 (比 clear 更快)
        box(stdscr, 0, 0);

        attron(COLOR_PAIR(2));
        mvprintw(1, 2, "LINUX FFT VISUALIZER");
        attroff(COLOR_PAIR(2));

        // --- 画柱状图的核心循环 ---
        attron(COLOR_PAIR(1));
        for (int i = 0; i < BARS; i++) {
            // 获取高度 (限制最大值)
            int height = (int)spectrum_heights[i];
            if (height > 20) height = 20; // 防止冲出边框

            // 从下往上画
            // 屏幕高度大概是 24行，我们在底部留点空，从 22 行开始往上画
            for (int h = 0; h < height; h++) {
                // x 坐标放大一点，让柱子宽一点
                mvaddch(22 - h, 4 + i*2, '#'); 
                mvaddch(22 - h, 4 + i*2 + 1, '#'); 
            }
        }
        attroff(COLOR_PAIR(1));

        refresh();

        int ch = getch();
        if (ch == 'q') keep_running = 0;
        else if (ch == ' ') is_paused = !is_paused;
    }

    pthread_join(thread_id, NULL);
    endwin();
    return 0;
}
