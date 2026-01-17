#include "raylib.h"

#include <complex.h>

#ifndef AUDIO_VISUALIZER_H
#define AUDIO_VISUALIZER_H

#define BACKGROUND (Color){7, 13, 22, 255}
#define PEAK_COLOR (Color){40, 100, 255, 255}
#define VALLEY_COLOR (Color){193, 32, 255, 255}

#define DISPLAY_WIDTH 1280
#define DISPLAY_HEIGHT 720

#define FFT_SIZE (1 << 11)
#define NUM_BARS 32
#define GAP_WIDTH 10

#define MAX_FILEPATH_RECORDED   4096

#define FONT_SIZE 92
#define FONT_SPACING 0

typedef float complex float_complex;

typedef struct {
    Music music;
    bool playing;
    bool pause;

    char *filepath;

    Font font;
  
    float in_raw[FFT_SIZE];
    float in_hann[FFT_SIZE];
    float_complex out_raw[FFT_SIZE];
    float out_log[FFT_SIZE];
    float out_smooth[NUM_BARS];
} Visualizer;

void init_vis(void);

void update_vis(void);

void render_vis(void);

void free_vis(void);

// Copied from: https://cp-algorithms.com/algebra/fft.html
void fft(float in[], float complex out[], const int n);

void fft_compress();

void pause_decay();

void callback(void *buffer, unsigned int frames);

#endif // AUDIO_VISUALIZER_H
