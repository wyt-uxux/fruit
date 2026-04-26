
#pragma once

#include <memory>
#include <vector>

namespace FruitGame
{
    class Effect
    {
    public:
        virtual ~Effect() = default;
        virtual void update(double dt) = 0;
        virtual void render() const = 0;
        virtual bool isAlive() const = 0;
    };

    class EffectManager
    {
    public:
        void add(std::unique_ptr<Effect> effect);
        void update(double dt);
        void render() const;
        void clear();

    private:
        std::vector<std::unique_ptr<Effect>> effects_;
    };
}
