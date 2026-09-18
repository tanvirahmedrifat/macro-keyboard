#ifndef APP_GAMES_H
#define APP_GAMES_H

#include "app_manager.h"

// Initialize the games sub-menu
void AppGames_Init();

// Render and logic loop for the games sub-menu
void AppGames_Update();

// Handle inputs for the games sub-menu
void AppGames_HandleInput(LogicalEvent ev);

// Force the games menu to return to menu state (if we were in a game)
// Used when exiting or switching games internally
void AppGames_ExitToMenu();

#endif // APP_GAMES_H
