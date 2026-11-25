// AI-assisted: see AI39
#pragma once
#include <SFML/Audio.hpp>
#include <string>
#include "ui/theme.hpp"

class Audio {
    public:
        bool load();
        void applyTheme(const theme& t);
        void update(float dt);
        void playStone();
        void playCapture();
        void playError();
        void setSfxVolume(float v);
        void setMusicOn(bool on);
        void setMusicVolume(float v);
        float getSfxVolume();
    private:
        bool loadSfx(sf::SoundBuffer& buf, sf::Sound& snd, const std::string& path);
        void startMusic(const std::string& path);

        sf::Music bgm;
        sf::SoundBuffer stonebuf, capturebuf, errorbuf;
        sf::Sound stone, capture, error;

        enum class state { None, Out, In };
        state musicState = state::None;
        float musicFade = 0.f;
        std::string pendingMusic, currentMusic;
        void applySfxVolume();
        float musicVolume = 40.f, sfxVolume = 70.f;
        static constexpr float coeff_error = 0.5f;
        bool musicOn = true, sfxOn = true;
};