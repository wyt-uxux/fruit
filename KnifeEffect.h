#pragma once

#include <Windows.h>

#include <memory>
#include <vector>

namespace FruitGame
{
    class KnifeEffect
    {
    public:
        virtual ~KnifeEffect() = default;

        virtual void update(double dt, bool slicing, bool hasCursor, const POINT& cursor) = 0;
        virtual void renderTrail(const std::vector<POINT>& smoothTrail) const = 0;
    };

    std::unique_ptr<KnifeEffect> createBambooKnifeEffect();
}
