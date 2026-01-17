#include "visualizer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef PI
#define PI acos(-1)
#endif

static Visualizer* v = NULL;

inline static float lerp(const float a, const float b, const float t) {
  return a * (1.0 - t) + (b * t);
}

inline static float amp(const float_complex z) {
    float a = crealf(z);
    float b = cimagf(z);
    return log10f(a*a + b*b);
}

static Color lerpHSV(const Color a, const Color b, const float t) {
    Vector3 hsv1 =  ColorToHSV(a);
    Vector3 hsv2 =  ColorToHSV(b);
    Vector3 final = {
        .x = lerp(hsv1.x, hsv2.x, t),
        .y = lerp(hsv1.y, hsv2.y, t),
        .z = lerp(hsv1.z, hsv2.z, t) 
    };

    return ColorFromHSV(final.x, final.y, final.z);
}

void init_vis(void) {
    v = malloc(sizeof(*v));

    if(!v) {
        fprintf(stderr, "Error: Unable to load visualizer!\n");
        exit(1);
    }

    v->pause = false;
    v->playing = false;

    v->filepath = (char *)calloc(MAX_FILEPATH_RECORDED, 1);

    v->font = LoadFontEx("font/RacingSansOne-Regular.ttf", FONT_SIZE, NULL, FONT_SPACING); //
    
    if (!IsFontValid(v->font)) {
        fprintf(stderr, "Error: Unable to load font!\n");
        free_vis();
        exit(1);
    }
    
    memset(v->in_raw, 0, sizeof(v->in_raw));
    memset(v->in_hann, 0, sizeof(v->in_hann));
    memset(v->out_raw, 0, sizeof(v->out_raw));
    memset(v->out_log, 0, sizeof(v->out_log));
    memset(v->out_smooth, 0, sizeof(v->out_smooth));
}

void update_vis(void) {
    if (IsFileDropped()) {
        FilePathList dropped_files = LoadDroppedFiles();
        TextCopy(v->filepath, dropped_files.paths[0]);
        UnloadDroppedFiles(dropped_files);

        printf("Hey the file was sent to you!\n %s\n", v->filepath);

        if (!v->playing) {
            v->playing = true;
        } else {
            StopMusicStream(v->music);
            UnloadMusicStream(v->music);
        }
        
        v->music = LoadMusicStream(v->filepath);
           
        if (!IsMusicValid(v->music)) {
            fprintf(stderr, "Error: Unable to load the music file!\n");
            free_vis();
            exit(1);
        }
        
        PlayMusicStream(v->music);
    }

    if(!v->playing) return;
  
    UpdateMusicStream(v->music);

    if (IsKeyPressed(KEY_SPACE)) {
        v->pause = !v->pause;

        if (v->pause) PauseMusicStream(v->music);
        else ResumeMusicStream(v->music);
    }
}

void render_vis(void) {
    const int w = GetScreenWidth();
    const int h = GetScreenHeight();

    if (!v->playing) {
        const char *text = "Drop the music file to play!";
        Vector2 pos = {
            .x = w / 2.f - MeasureTextEx(v->font, text, (float)FONT_SIZE, FONT_SPACING).x/2,
            .y = h / 2.f - MeasureTextEx(v->font, text, (float)FONT_SIZE, FONT_SPACING).y/2,
        };
   
        DrawTextEx(v->font, text, pos, FONT_SIZE, FONT_SPACING, (Color){255, 255, 255, 255});
        return;
    }
    
    const float min_height = 0.01f;
    const float cell_width = (float)(w - (NUM_BARS + 1) * GAP_WIDTH) / NUM_BARS;
    for (size_t i = 0; i < NUM_BARS; i++) {
        float t = v->out_smooth[i] > min_height ? v->out_smooth[i] : min_height;
        Color l = lerpHSV(VALLEY_COLOR, PEAK_COLOR, t);

        Vector2 start_pos = {
            .x = i * cell_width,
            .y = h - h*3/4*t
        };

        Vector2 end_pos = {
            .x = cell_width,
            .y = h*3/4*t
        };
        
        DrawRectangleGradientV(start_pos.x + GAP_WIDTH * (i + 1), start_pos.y, end_pos.x, end_pos.y, l, VALLEY_COLOR);
    }
}

void free_vis(void) {
    if(IsMusicValid(v->music)) UnloadMusicStream(v->music);
    free(v->filepath);
    free(v);
}

void pause_decay() {
    const float release = 0.04f;
    for (size_t i = 0; i < NUM_BARS; i++) {
        if (v->out_log[i] < 0.f) {
            v->out_log[i] = 0.f;
            continue;
        }
        
        v->out_smooth[i] *= (1.0f - release);
    }
}

// Copied from: https://cp-algorithms.com/algebra/fft.html
void fft(float in[], float_complex out[], const int n) {
    for(int i = 0; i < n; i++) out[i] = in[i];
  
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;

        if (i < j) {
            float_complex temp = out[i];
            out[i] = out[j];
            out[j] = temp;
        }
    }

    for (int len = 2; len <= n; len <<= 1) {
        float ang = -2 * PI / len;
        float_complex wlen = cos(ang) + sin(ang) * I;
        for (int i = 0; i < n; i += len) {
            float_complex w = 1;
            for (int j = 0; j < len / 2; j++) {
                float_complex u = out[i+j], v = out[i+j+len/2] * w;
                out[i+j] = u + v;
                out[i+j+len/2] = u - v;
                w *= wlen;
            }
        }
    }
}

void fft_compress() {
    // Applying Hann's window for smaller spectral leakage: https://www.youtube.com/watch?v=JZRTJRTnYNU
    for(size_t i = 0; i < FFT_SIZE; i++) {
        float multiplier = 0.5 - 0.5 * (1 - cosf(2*PI*i/FFT_SIZE));
        v->in_hann[i] = multiplier * v->in_raw[i];
    }

    fft(v->in_hann, v->out_raw, FFT_SIZE);

    size_t m = 0;
    float max_amp = 1.0f;
    const float step = 1.14f;
    for(float f = 1.0f; (size_t)f < FFT_SIZE / 2; f = ceilf(f * step)) {
        float end = ceilf(f * step);
        float a = 0.f;
        for (size_t q = (size_t)f; q < FFT_SIZE / 2 && q < end; q++) {
            float b = amp(v->out_raw[q]);
            if (b > a) a = b;
        }

        if (max_amp < a) max_amp = a;
        v->out_log[m++] = a;
    }

    for (size_t i = 0; i < m; ++i) {
        v->out_log[i] /= max_amp;
    }
    
    for (size_t i = 0; i < NUM_BARS; i++) {
        const float attack = 0.62f;
        const float release = 0.28f;

        if (v->out_log[i] < 0.f) {
            v->out_log[i] = 0.f;
            continue;
        }
        
        if (v->out_smooth[i] > v->out_log[i])
            v->out_smooth[i] += (v->out_log[i] - v->out_smooth[i]) * attack;
        else
            v->out_smooth[i] += (v->out_log[i] - v->out_smooth[i]) * release;
    }
}

void callback(void *buffer, unsigned int frames) {
    float *samples = (float *)buffer;
    
    const int size = frames > FFT_SIZE? FFT_SIZE : frames;
    for(int frame = 0; frame < size; frame++) {
        float* left = &samples[frame * 2 + 0];
        float* right = &samples[frame * 2 + 1];

        v->in_raw[frame] = (*left + *right) / 2;
    }
    
    if (v->pause) {
        pause_decay();
        return;
    }
    
    fft_compress();
}
