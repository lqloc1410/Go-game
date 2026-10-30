#pragma once
// AI-assisted: see AI43, AI44
enum class gameMode { PvP, PvAI };
enum class difficulty { Easy, Medium, Hard };

struct gameCfg {
    gameMode mode = gameMode::PvP;
    difficulty difficulty = difficulty::Easy;
    int     boardSize = 9;
};