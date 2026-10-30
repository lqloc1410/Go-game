// AI-assisted: see AI39
#pragma once
#include <SFML/Audio.hpp>

class Audio {
    public:
        bool load();
        void playStone();
        void playCapture();
        void playError();
        void setSfxVolume(float v);
        void setMusicOn(bool on);
        void setMusicVolume(float v);
        float getSfxVolume();
    private:
        sf::Music bgm;
        sf::SoundBuffer stonebuf, capturebuf, errorbuf;
        sf::Sound stone, capture, error;
        void applySfxVolume();
        float sfxVolume = 70.f;
        static constexpr float coeff_error = 0.5f;
};