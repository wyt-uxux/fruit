#pragma once

#include <Windows.h>

#include <memory>

#include "Effect.h"

namespace FruitGame
{
    std::unique_ptr<Effect> makeComboTextEffect(POINT impactPoint, int comboCount);
}
