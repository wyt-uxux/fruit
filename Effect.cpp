#include "Effect.h"

#include <algorithm>

namespace FruitGame
{
    void EffectManager::add(std::unique_ptr<Effect> effect)
    {
        if (effect)
        {
            effects_.push_back(std::move(effect));
        }
    }

    void EffectManager::update(double dt)
    {
        for (auto& effect : effects_)
        {
            effect->update(dt);
        }

        effects_.erase(
            std::remove_if(effects_.begin(), effects_.end(), [](const std::unique_ptr<Effect>& effect)
                { return !effect->isAlive(); }),
            effects_.end());
    }

    void EffectManager::render() const
    {
        for (const auto& effect : effects_)
        {
            effect->render();
        }
    }

    void EffectManager::clear()
    {
        effects_.clear();
    }
}
