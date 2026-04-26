#pragma once

#include <Windows.h>

#include <memory>
#include <vector>

#include <graphics.h>

#include "Effect.h"

namespace FruitGame
{
    std::unique_ptr<Effect> makeFlashEffect(const IMAGE* flashSprite, POINT impactPoint, double angle);
    std::unique_ptr<Effect> makeJuiceEffect(POINT impactPoint, COLORREF color);
}

