// AI-assisted: see AI39
#include <iostream>
#include <algorithm>
#include <random>
#include <ui/audio.hpp>

bool Audio::loadSfx(sf::SoundBuffer& buf, sf::Sound& snd, const std::string& path) {
    if (!buf.loadFromFile(path)) { std::cerr << "Could not load sound: " << path << "\n"; return false; }
    snd.setBuffer(buf);
    return true;
}

void Audio::startMusic(const std::string& path) {
    if (!bgm.openFromFile(path)) { std::cerr << "Could not open music: " << path << "\n"; currentMusic.clear(); return; }
    bgm.setLoop(true);
    currentMusic = path;
    if (musicOn) bgm.play();
}

bool Audio::load() { applyTheme(themeManager::instance().current()); return sfxOn; }
void Audio::applyTheme(const theme& t) {

    if (currentMusic.empty() && musicState == state::None) {            // first call: start right away
        bgm.setVolume(musicVolume);
        startMusic(t.music);
    } else if (t.music != currentMusic) {                         // later: fade out, swap, fade in
        pendingMusic = t.music;
        musicState = state::Out;
        musicFade = 0.f;
    }
}

void Audio::update(float dt) {
    if (musicState == state::None) return;
    constexpr float kmusicState = 0.35f;
    musicFade += dt;
    const float k = std::min(musicFade / kmusicState, 1.f);
    if (musicState == state::Out) {
        bgm.setVolume(musicVolume * (1.f - k));
        if (k >= 1.f) { startMusic(pendingMusic); bgm.setVolume(0.f); musicState = state::In; musicFade = 0.f; }
    } else {
        bgm.setVolume(musicVolume * k);
        if (k >= 1.f) musicState = state::None;
    }
}

void Audio::playStone() { stone.play(); }
void Audio::playCapture() { capture.play(); }
void Audio::playError() { error.play(); }

void Audio::setMusicOn(bool on) {
    musicOn = on;
    if (on) {
        if (bgm.getStatus() != sf::SoundSource::Playing) bgm.play();
    }
    else bgm.pause();
}

void Audio::setMusicVolume(float v) {
    musicVolume = std::clamp(v, 0.f, 100.f);
    if (musicState == state::None) bgm.setVolume(musicVolume);
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
