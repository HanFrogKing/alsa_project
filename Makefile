CC = gcc
CFLAGS = -Wall -O2
LIBS_ALSA = -lasound
LIBS_UI = -lncurses -lpthread
LIBS_MATH = -lm
LIBS_FFT = -lfftw3

all: loop visualizer generator

# 1. 回声机
loop: alsa_loopback.c
	$(CC) alsa_loopback.c -o alsa_loop $(LIBS_ALSA)

# 2. 频谱仪 (最复杂的依赖)
visualizer: visualizer.c
	$(CC) visualizer.c -o visualizer $(LIBS_ALSA) $(LIBS_UI) $(LIBS_FFT) $(LIBS_MATH)

# 3. 音乐生成器
generator: gen_music_poly.c
	$(CC) gen_music_poly.c -o gen_music_poly $(LIBS_MATH)

clean:
	rm -f alsa_loop visualizer gen_music_poly
