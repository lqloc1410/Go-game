#pragma once

enum class gameMode { PvP, PvAI };
enum class difficulty { Easy, Medium, Hard };

struct gameCfg {
    gameMode mode = gameMode::PvP;
    difficulty difficulty = difficulty::Easy;
    int     boardSize = 9;
};