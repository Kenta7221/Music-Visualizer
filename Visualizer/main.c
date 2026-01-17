#include "raylib.h"

#include "visualizer.h"

int main(void) {
    InitWindow(DISPLAY_WIDTH, DISPLAY_HEIGHT, "Audio Visualizer");

    InitAudioDevice();
    AttachAudioMixedProcessor(callback);

    init_vis();

    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        update_vis();
      
        BeginDrawing();
        ClearBackground(BACKGROUND);

        render_vis();

        EndDrawing();
    }
    
    free_vis();
  
    DetachAudioMixedProcessor(callback);
    CloseAudioDevice();
  
    CloseWindow();

    return 0;
}
