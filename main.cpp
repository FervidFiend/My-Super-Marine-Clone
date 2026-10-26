#include "Game.h"

// ---------------- Main ----------------
int main(int argc, char **argv) {
  bool wideCheck = argc > 1 && std::string(argv[1]) == "--self-test-wide";
  InitWindow(wideCheck ? 1280 : 550, wideCheck ? 800 : 420, "Super Marine");
  EnableVsync();
  InitGame();
  if (argc > 1 && (std::string(argv[1]) == "--self-test" || wideCheck)) {
    bool ok = RunGameChecks();
    ShutdownGame();
    CloseWindow();
    return ok ? 0 : 2;
  }
  // Capture reference scenes, 12 shots per scene at quarter-second intervals.
  if (argc > 2 && std::string(argv[1]) == "--capture-screens") {
    const std::pair<int, const char *> screens[] = {
        {2, "instructions"}, {1, "difficulty"}, {7, "scores"},
        {0, "title"}, {5, "help"}, {6, "gameplay"},
        {8, "victory"}, {9, "defeat"}};
    for (auto [index, name] : screens) {
      ConfigureGamePreview(index);
      for (int tick = 0, shot = 0; shot < 12; ++tick) {
        PollCustomEvents();
        UpdateGame(1 / 24.0f);
        BeginDrawing();
        DrawGame();
        if (tick % 6 == 0) {
          auto s = GetCurrentWindowSize();
          char file[64];
          snprintf(file, sizeof file, "/%s_%02d.png", name, shot++);
          SaveScreenshot(int(s.x), int(s.y), std::string(argv[2]) + file);
        }
        EndDrawing();
      }
    }
    ShutdownGame();
    CloseWindow();
    return 0;
  }

  // ---------------- MAIN LOOP ----------------
  while (!WindowShouldClose()) {
    PollCustomEvents();
    float dt = GetFrameTime();

    UpdateGame(dt);

    BeginDrawing();
    DrawGame();

    EndDrawing();
  }

  ShutdownGame();
  CloseWindow();
  return 0;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, PSTR, int) {
  return main(__argc, __argv);
}
