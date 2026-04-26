#pragma once

#include <Windows.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <random>
#include <vector>

#include <graphics.h>

#include "WaveController.h"

namespace FruitGame
{
    enum class CutSfxKind
    {
        Splatter,
        Splatter2,
        Banana
    };

    inline constexpr int kWindowWidth = 960;
    inline constexpr int kWindowHeight = 540;

    struct Vec2
    {
        double x = 0.0;
        double y = 0.0;
    };

    struct FruitSpriteSet
    {
        const IMAGE* whole = nullptr;
        const IMAGE* half1 = nullptr;
        const IMAGE* half2 = nullptr;
        COLORREF juiceColor = RGB(255, 255, 255);
        bool isBomb = false;
        CutSfxKind cutSfx = CutSfxKind::Splatter2;
    };

    struct SliceEvent
    {
        POINT impactPoint{};
        double angle = 0.0;
        COLORREF juiceColor = RGB(255, 255, 255);
        CutSfxKind cutSfx = CutSfxKind::Splatter2;
    };

    class FruitSpawnController
    {
    public:
        void update(double dt);
        [[nodiscard]] bool tryConsumeBatch(SpawnBatch& batch);
        void setDifficultyTier(int tier) noexcept;

    private:
        WaveController waveController_;
    };

    class Fruit
    {
    public:
        Fruit(const FruitSpriteSet* spriteSet, const IMAGE* shadowSprite, Vec2 position, Vec2 velocity, double gravity, double angularVelocity);

        void update(double dt);
        void drawShadow() const;
        void drawFruit() const;

        [[nodiscard]] bool isOffScreen() const;
        [[nodiscard]] RECT bounds() const;
        [[nodiscard]] const Vec2& position() const noexcept;
        [[nodiscard]] const Vec2& velocity() const noexcept;
        [[nodiscard]] double radius() const noexcept;
        [[nodiscard]] double angle() const noexcept;
        [[nodiscard]] double angularVelocity() const noexcept;
        [[nodiscard]] bool hitTest(const POINT& p, double expandScale) const;
        [[nodiscard]] const FruitSpriteSet* spriteSet() const noexcept;
        [[nodiscard]] bool isBomb() const noexcept;

    private:
        const FruitSpriteSet* spriteSet_ = nullptr;
        const IMAGE* shadowSprite_ = nullptr;
        Vec2 position_;
        Vec2 velocity_;
        double gravity_ = 0.0;
        double angle_ = 0.0;
        double angularVelocity_ = 0.0;
        int width_ = 0;
        int height_ = 0;
        double radius_ = 0.0;
    };

    class FruitField
    {
    public:
        using SpawnCallback = std::function<void(int)>;
        using BombHitCallback = std::function<void(POINT)>;

        explicit FruitField(std::vector<const FruitSpriteSet*> sprites,
            const FruitSpriteSet* bombSprite,
            const IMAGE* backgroundSprite,
            const IMAGE* shadowSprite);

        void seedInitialFruits(std::size_t count);
        void update(double dt);
        void render() const;
        int trySliceAlongSegment(const POINT& from, const POINT& to, double bladeAngle, std::vector<SliceEvent>* events);
        [[nodiscard]] int missedCount() const noexcept;
        int consumeSpawnedSinceLastFrame() noexcept;
        void setSpawnCallback(SpawnCallback callback);
        void setBombHitCallback(BombHitCallback callback);
        void setSpawnEnabled(bool enabled) noexcept;
        void setDifficultyTier(int tier) noexcept;

        [[nodiscard]] const std::vector<Fruit>& fruits() const noexcept;
        [[nodiscard]] std::vector<Fruit>& fruits() noexcept;

    private:
        void spawnOneRandomFruit();
        void spawnOneBomb();
        void spawnVerticalGroup(int count, bool sameFruit);
        void spawnScatterGroup(int count);

        const IMAGE* backgroundSprite_ = nullptr;
        const IMAGE* shadowSprite_ = nullptr;
        std::vector<const FruitSpriteSet*> spriteSets_;
        const FruitSpriteSet* bombSpriteSet_ = nullptr;
        std::vector<Fruit> fruits_;
        struct FruitHalf
        {
            const IMAGE* sprite = nullptr;
            Vec2 position{};
            Vec2 velocity{};
            double angle = 0.0;
            double angularVelocity = 0.0;
            int width = 0;
            int height = 0;

            void update(double dt, double gravity);
            void draw() const;
            bool isOffScreen() const;
        };
        std::vector<FruitHalf> slicedHalves_;
        int missedCount_ = 0;
        int spawnedSinceLastFrame_ = 0;
        bool spawnEnabled_ = true;
        SpawnCallback spawnCallback_;
        BombHitCallback bombHitCallback_;
        int spawnWaveCount_ = 0;
        bool earlyBombWaveUsed_ = false;
        std::mt19937 rng_;
        FruitSpawnController spawnController_;
        int difficultyTier_ = 0;
    };
}
