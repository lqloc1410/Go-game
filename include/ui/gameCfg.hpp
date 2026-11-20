#pragma once
// AI-assisted: see AI43, AI44

enum class gameMode { PvP, PvAI };
enum class difficulty { Easy, Medium, Hard };

enum class ruleSet { Japanese, Chinese, AGA };
enum class boardTheme { ClassicWood, Bamboo, Slate };   // placeholder themes
enum class stoneStyle { Classic, SlateAndShell, Modern }; // placeholder styles

inline float defaultKomi(ruleSet r) {
    switch (r) {
        case ruleSet::Japanese: return 6.5f;
        case ruleSet::Chinese: return 7.5f;
        case ruleSet::AGA: return 7.5f;
    }
    return 7.5f;
}

struct gameCfg {
    gameMode mode = gameMode::PvP;
    difficulty difficulty = difficulty::Easy;
    int boardSize = 9;
    int handicap = 0;
    float komi = 6.5f; //default: Japanese rule set

    // Settings
    bool bgmOn = true;
    bool sfxOn = true;
    boardTheme theme = boardTheme::ClassicWood;
    stoneStyle stones = stoneStyle::Classic;
    ruleSet rules = ruleSet::Japanese;

    void setRuleSet(ruleSet r) {
        rules = r;
        komi = defaultKomi(r);
    }

    void resetSettings() {
        bgmOn = true;
        sfxOn = true;
        theme = boardTheme::ClassicWood;
        stones = stoneStyle::Classic;
        rules = ruleSet::Japanese;
    }
};