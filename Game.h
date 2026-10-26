#pragma once

#include "MyEngine.h"

void InitGame();
void UpdateGame(float deltaTime);
void DrawGame();
void ShutdownGame();
// Runs deterministic gameplay checks, then resets to a playable mission.
bool RunGameChecks();
// Selects one of the presentation scenes for the framebuffer checks.
std::string ConfigureGamePreview(int index);
