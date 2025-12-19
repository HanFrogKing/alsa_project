#include <stdio.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include <ncurses.h>
#include <pthread.h>  // 引入多线程库

// --- 全局变量 (用于线程间通信) ---
volatile int keep_running = 1; // 控制程序是否退出
volatile int is_paused = 0;    // 控制暂停/播放

// --- 音频线程工人：专门负责干脏活累活 ---
void *audio_thread_func(void *arg) {
    int rc;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    unsigned int val = 44100;
    int dir;
    snd_pcm_uframes_t frames = 32;
    char *buffer;
    int size;

    // 打开刚才录好的 output.wav (确保你有这个文件，或者改名)
    FILE *fp = fopen("output.wav", "rb");
    if (!fp) return NULL;
    fseek(fp, 44, SEEK_SET); // 跳过 WAV 头

    // 打开 ALSA 设备
    rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) return NULL;

    // 设置参数 (快速简写版)
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, params, 2);
    snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
    snd_pcm_hw_params(handle, params);

    // 准备缓冲区
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    size = frames * 4;
    buffer = (char *) malloc(size);

    // --- 音频循环 ---
    while (keep_running) {
        if (is_paused) {
            // 如果暂停了，就歇会儿 (睡100毫秒)，别占 CPU
            usleep(100000);
            continue;
        }

        // 读文件
        if (fread(buffer, 1, size, fp) == 0) {
            // 读完了，从头循环播放
            fseek(fp, 44, SEEK_SET);
            continue;
        }

        // 写声卡
        rc = snd_pcm_writei(handle, buffer, frames);
        if (rc == -EPIPE) {
            snd_pcm_prepare(handle);
        }
    }

    // 清理工作
    fclose(fp);
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    free(buffer);
    return NULL;
}

// --- 主线程：负责界面和指挥 ---
int main() {
    pthread_t thread_id;

    // 1. 启动音频线程
    // pthread_create(线程ID指针, 属性, 线程函数, 参数)
    pthread_create(&thread_id, NULL, audio_thread_func, NULL);

    // 2. 初始化 Ncurses 界面
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    timeout(100); // 关键！getch 只等待 100ms，超时就返回 (防止彻底卡死)

    // 3. UI 循环
    while (keep_running) {
        // --- 画图 ---
        clear(); // 清屏
        box(stdscr, 0, 0); // 画框框

        mvprintw(2, 4, "=== SUPER COOL ALSA PLAYER ===");
        mvprintw(4, 4, "Status: %s", is_paused ? "[ PAUSED ]" : "[ PLAYING ]");
        mvprintw(6, 4, "Key Controls:");
        mvprintw(7, 6, "[Space] : Pause / Resume");
        mvprintw(8, 6, "[ q ]   : Quit");
        
        // 画个假装的进度条（让它动起来）
        static int bar_len = 0;
        if (!is_paused) bar_len = (bar_len + 1) % 40;
        mvprintw(10, 4, "[");
        for(int i=0; i<40; i++) {
            if (i < bar_len) addch('=');
            else addch(' ');
        }
        addch(']');

        refresh(); // 提交渲染

        // --- 处理按键 ---
        int ch = getch();
        if (ch == 'q') {
            keep_running = 0; // 通知音频线程退出
        } else if (ch == ' ') {
            is_paused = !is_paused; // 切换暂停状态
        }
    }

    // 等待音频线程彻底结束
    pthread_join(thread_id, NULL);
    
    // 退出界面
    endwin();
    return 0;
}
