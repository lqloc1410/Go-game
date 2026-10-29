#include <iostream>
#include <ui/audio.hpp>

#if defined(_WIN32) && defined(_UCRT)
#include <stdio.h>
extern "C" {
    static FILE* __cdecl ucrt_iob_func(void) {
        static FILE files[3];
        files[0] = *stdin;
        files[1] = *stdout;
        files[2] = *stderr;
        return files;
    }
    void* __imp___iob_func = (void*)&ucrt_iob_func;
}
#endif

static bool loadSfx(sf::SoundBuffer& buf, sf::Sound& snd, const char* path) {
    if (!buf.loadFromFile(path)) {
        std::cerr << "Could not load sound: " << path << '\n';
        return false;
    }
    snd.setBuffer(buf);
    return true;
}


bool Audio::load() {
    bool check = true;
    check &= loadSfx(stonebuf, stone, "assets/audio/stone.wav");
    check &= loadSfx(capturebuf, capture, "assets/audio/capture.wav");
    check &= loadSfx(errorbuf, error, "assets/audio/error.wav");


    if (!bgm.openFromFile("assets/audio/bgm.ogg")) {
        std::cerr << "Could not open background music file" << '\n';
        check = false;
    }
    else {
        bgm.setLoop(true);
        bgm.setVolume(40.f);
        bgm.play();
    }

    return check;
}

void Audio::playStone() { stone.play(); }
void Audio::playCapture() { capture.play(); }
void Audio::playError() { error.play(); }

void Audio::setMusicOn(bool on) {
    if (on) {
        if (bgm.getStatus() != sf::SoundSource::Playing) bgm.play();
    }
    else bgm.pause();
}

void Audio::setMusicVolume(float v) {
    bgm.setVolume(v);
}

void Audio::setSfxVolume(float v) {
    if (v > 100.f) v = 100.f;
    else if (v < 0.f) v = 0.f;
    sfxVolume = v;
    applySfxVolume();
}
void Audio::applySfxVolume() {
    stone.setVolume(sfxVolume);
    capture.setVolume(sfxVolume);
    error.setVolume(sfxVolume * coeff_error);
}
