#pragma once
#include <SFML/Graphics.hpp>

constexpr float kVirtualW = 1150.f;
constexpr float kVirtualH = 800.f;

inline sf::View makeLetterboxView(sf::Vector2u windowSize) {
    float window_ratio = windowSize.x / static_cast<float>(windowSize.y);
    float view_ratio = kVirtualW / kVirtualH;

    float sizeX = 1.f, sizeY = 1.f, posX = 0.f, posY = 0.f;
    if (window_ratio > view_ratio) {
        sizeX = view_ratio / window_ratio;
        posX = (1.f - sizeX) / 2.f;
    }
    else {
        sizeY = window_ratio / view_ratio;
        posY = (1.f - sizeY) / 2.f;
    }

    sf::View view(sf::FloatRect(0.f, 0.f, kVirtualW, kVirtualH));
    view.setViewport(sf::FloatRect(posX, posY, sizeX, sizeY));
    return view;
}