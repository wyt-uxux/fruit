#define NOMINMAX
#include "Fruit.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <string>

namespace FruitGame
{
    namespace
    {
        constexpr double kGravity = 1520.0;
        constexpr std::size_t kMaxFruits = 14;
        constexpr double kTwoPi = 6.28318530717958647692;

        double difficultySpeedMultiplier(int tier)
        {
            switch (std::max(0, std::min(tier, 3)))
            {
            case 0:
                return 1.0;
            case 1:
                return 1.01;
            case 2:
                return 1.02;
            default:
                return 1.03;
            }
        }

        double clampSpawnX(double x)
        {
            return std::max(112.0, std::min(x, static_cast<double>(kWindowWidth) - 112.0));
        }

        double biasVelocityInward(double startX, double vx, std::mt19937& rng)
        {
            constexpr double kEdgeBand = 210.0;
            constexpr double kInwardMin = 90.0;
            std::uniform_real_distribution<double> inwardBoost(0.0, 70.0);

            if (startX < kEdgeBand)
            {
                if (vx < kInwardMin)
                {
                    vx = kInwardMin + inwardBoost(rng);
                }
            }
            else if (startX > static_cast<double>(kWindowWidth) - kEdgeBand)
            {
                if (vx > -kInwardMin)
                {
                    vx = -(kInwardMin + inwardBoost(rng));
                }
            }

            return vx;
        }

        int clampInt(int value, int low, int high)
        {
            return std::max(low, std::min(value, high));
        }

        double distanceSqPointToSegment(const Vec2& p, const Vec2& a, const Vec2& b)
        {
            const double abx = b.x - a.x;
            const double aby = b.y - a.y;
            const double apx = p.x - a.x;
            const double apy = p.y - a.y;
            const double ab2 = abx * abx + aby * aby;

            if (ab2 <= 1e-9)
            {
                return apx * apx + apy * apy;
            }

            const double t = std::max(0.0, std::min(1.0, (apx * abx + apy * aby) / ab2));
            const double cx = a.x + abx * t;
            const double cy = a.y + aby * t;
            const double dx = p.x - cx;
            const double dy = p.y - cy;
            return dx * dx + dy * dy;
        }

        inline unsigned char getA(DWORD c)
        {
            return static_cast<unsigned char>((c >> 24) & 0xFF);
        }

        inline unsigned char getR(DWORD c)
        {
            return static_cast<unsigned char>((c >> 16) & 0xFF);
        }

        inline unsigned char getG(DWORD c)
        {
            return static_cast<unsigned char>((c >> 8) & 0xFF);
        }

        inline unsigned char getB(DWORD c)
        {
            return static_cast<unsigned char>(c & 0xFF);
        }

        inline DWORD makeARGB(unsigned char a, unsigned char r, unsigned char g, unsigned char b)
        {
            return (static_cast<DWORD>(a) << 24) |
                (static_cast<DWORD>(r) << 16) |
                (static_cast<DWORD>(g) << 8) |
                static_cast<DWORD>(b);
        }

        void drawRotatedImageHQ(const IMAGE& src, int centerX, int centerY, double angle)
        {
            const int srcW = src.getwidth();
            const int srcH = src.getheight();
            if (srcW <= 0 || srcH <= 0)
            {
                return;
            }

            DWORD* dstBuf = GetImageBuffer();
            DWORD* srcBuf = GetImageBuffer(const_cast<IMAGE*>(&src));
            if (dstBuf == nullptr || srcBuf == nullptr)
            {
                return;
            }

            const int dstW = getwidth();
            const int dstH = getheight();
            const double cx = static_cast<double>(srcW) * 0.5;
            const double cy = static_cast<double>(srcH) * 0.5;
            const double c = std::cos(angle);
            const double s = std::sin(angle);

            for (int sy = 0; sy < srcH; ++sy)
            {
                const double ry = static_cast<double>(sy) - cy;
                for (int sx = 0; sx < srcW; ++sx)
                {
                    const DWORD srcPx = srcBuf[sy * srcW + sx];

                    unsigned char a = getA(srcPx);
                    if (a == 0)
                    {
                        const unsigned char r = getR(srcPx);
                        const unsigned char g = getG(srcPx);
                        const unsigned char b = getB(srcPx);
                        if (r < 8 && g < 8 && b < 8)
                        {
                            continue;
                        }
                        a = 255;
                    }

                    unsigned char sr = getR(srcPx);
                    unsigned char sg = getG(srcPx);
                    unsigned char sb = getB(srcPx);

                    if (a < 255)
                    {
                        const int aa = std::max(1, static_cast<int>(a));
                        sr = static_cast<unsigned char>(std::min(255, (static_cast<int>(sr) * 255 + aa / 2) / aa));
                        sg = static_cast<unsigned char>(std::min(255, (static_cast<int>(sg) * 255 + aa / 2) / aa));
                        sb = static_cast<unsigned char>(std::min(255, (static_cast<int>(sb) * 255 + aa / 2) / aa));
                    }

                    const double rx = static_cast<double>(sx) - cx;
                    const int dx = centerX + static_cast<int>(std::round(rx * c - ry * s));
                    const int dy = centerY + static_cast<int>(std::round(rx * s + ry * c));

                    if (dx < 0 || dx >= dstW || dy < 0 || dy >= dstH)
                    {
                        continue;
                    }

                    DWORD& dstPx = dstBuf[dy * dstW + dx];
                    if (a >= 250)
                    {
                        dstPx = makeARGB(0xFF, sr, sg, sb);
                        continue;
                    }

                    const unsigned char dr = getR(dstPx);
                    const unsigned char dg = getG(dstPx);
                    const unsigned char db = getB(dstPx);

                    const int ia = 255 - a;
                    const unsigned char rr = static_cast<unsigned char>((sr * a + dr * ia) / 255);
                    const unsigned char rg = static_cast<unsigned char>((sg * a + dg * ia) / 255);
                    const unsigned char rb = static_cast<unsigned char>((sb * a + db * ia) / 255);

                    dstPx = makeARGB(0xFF, rr, rg, rb);
                }
            }
        }

        void drawRotatedFruitImage(const IMAGE& src, int centerX, int centerY, double angle)
        {
            const int srcW = src.getwidth();
            const int srcH = src.getheight();
            if (srcW <= 0 || srcH <= 0)
            {
                return;
            }

            IMAGE rotated;
            const COLORREF keyColor = BLACK;
            rotateimage(&rotated, const_cast<IMAGE*>(&src), angle, keyColor, true, true);

            const int width = rotated.getwidth();
            const int height = rotated.getheight();
            if (width <= 0 || height <= 0)
            {
                return;
            }

            IMAGE mask(width, height);
            IMAGE source(width, height);

            DWORD* rotatedBuf = GetImageBuffer(&rotated);
            DWORD* maskBuf = GetImageBuffer(&mask);
            DWORD* sourceBuf = GetImageBuffer(&source);
            if (rotatedBuf == nullptr || maskBuf == nullptr || sourceBuf == nullptr)
            {
                return;
            }

            const int keyR = GetRValue(keyColor);
            const int keyG = GetGValue(keyColor);
            const int keyB = GetBValue(keyColor);

            for (int i = 0; i < width * height; ++i)
            {
                const DWORD px = rotatedBuf[i];
                const int r = static_cast<int>(getR(px));
                const int g = static_cast<int>(getG(px));
                const int b = static_cast<int>(getB(px));
                const int dr = std::abs(static_cast<int>(getR(px)) - keyR);
                const int dg = std::abs(static_cast<int>(getG(px)) - keyG);
                const int db = std::abs(static_cast<int>(getB(px)) - keyB);

                // rotateimage anti-aliasing creates dark fringe around black fill, so
                // clear both exact black and near-black spill after rotation.
                const bool isKeyColor = (dr <= 12 && dg <= 12 && db <= 12);
                const bool isBlackSpill = (r <= 36 && g <= 36 && b <= 36);
                const bool transparent = isKeyColor || isBlackSpill;

                if (transparent)
                {
                    maskBuf[i] = 0x00FFFFFF;
                    sourceBuf[i] = 0xFF000000;
                    continue;
                }

                maskBuf[i] = 0xFF000000;
                sourceBuf[i] = 0xFF000000 |
                    (static_cast<DWORD>(getR(px)) << 16) |
                    (static_cast<DWORD>(getG(px)) << 8) |
                    static_cast<DWORD>(getB(px));
            }

            const int drawX = centerX - rotated.getwidth() / 2;
            const int drawY = centerY - rotated.getheight() / 2;
            putimage(drawX, drawY, &mask, SRCAND);
            putimage(drawX, drawY, &source, SRCPAINT);
        }

        Vec2 rotateLocal(const Vec2& local, double angle)
        {
            const double c = std::cos(angle);
            const double s = std::sin(angle);
            return Vec2{ local.x * c - local.y * s, local.x * s + local.y * c };
        }

        void drawBombFlame(const Vec2& fruitPos, double fruitAngle)
        {
            const double t = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
            const double visualAngle = -fruitAngle;

            // Fuse anchor is near top-left in boom.png, matching original JS offsets (-22, -21).
            const Vec2 fuseLocal{ -22.0, -21.0 };
            const Vec2 fuseWorldOffset = rotateLocal(fuseLocal, visualAngle);
            const Vec2 fusePos{ fruitPos.x + fuseWorldOffset.x, fruitPos.y + fuseWorldOffset.y };

            // Flame leans out along the fuse direction so visual orientation rotates with bomb.
            Vec2 flameDir = rotateLocal(Vec2{ -0.78, -0.62 }, visualAngle);
            const double dirLen = std::max(1e-6, std::sqrt(flameDir.x * flameDir.x + flameDir.y * flameDir.y));
            flameDir.x /= dirLen;
            flameDir.y /= dirLen;

            const Vec2 sideDir{ -flameDir.y, flameDir.x };
            const Vec2 base{
                fusePos.x + flameDir.x * 2.2,
                fusePos.y + flameDir.y * 2.2 };

            // Draw a layered flame core + halo for a cleaner high-quality burning look.
            for (int layer = 0; layer < 11; ++layer)
            {
                const double p = static_cast<double>(layer) / 10.0;
                const double w = (1.0 - p);
                const double along = 3.0 + p * (13.0 + 2.0 * std::sin(t * 9.0));
                const double side = (0.8 + 2.8 * w) * std::sin(t * 15.0 + p * 4.6);

                const int x = static_cast<int>(std::round(base.x + flameDir.x * along + sideDir.x * side));
                const int y = static_cast<int>(std::round(base.y + flameDir.y * along + sideDir.y * side));

                const int rr = static_cast<int>(255);
                const int rg = static_cast<int>(138 + 100 * w);
                const int rb = static_cast<int>(28 + 38 * w);

                setfillcolor(RGB(rr, rg, rb));
                solidcircle(x, y, static_cast<int>(std::round(1.0 + 2.4 * w)));
            }

            for (int i = 0; i < 14; ++i)
            {
                const double seed = static_cast<double>(i) * 0.71;
                const double cycle = std::fmod(t * 3.2 + seed, 1.0);
                const double rise = 1.0 - cycle;
                const double spread = (1.2 + 2.2 * rise) * std::sin((t * 18.0) + seed * 7.0);

                const Vec2 p{
                    base.x + flameDir.x * (6.0 + 16.0 * rise) + sideDir.x * spread,
                    base.y + flameDir.y * (6.0 + 16.0 * rise) + sideDir.y * spread };

                const bool core = (i % 3) != 0;
                setfillcolor(core ? RGB(255, 214, 96) : RGB(255, 106, 22));
                solidcircle(static_cast<int>(std::round(p.x)), static_cast<int>(std::round(p.y)), core ? 2 : 1);
            }

            setfillcolor(RGB(255, 245, 210));
            solidcircle(static_cast<int>(std::round(base.x)), static_cast<int>(std::round(base.y)), 2);
        }

    }

    void FruitSpawnController::update(double dt)
    {
        waveController_.update(dt);
    }

    bool FruitSpawnController::tryConsumeBatch(SpawnBatch& batch)
    {
        return waveController_.tryConsumeBatch(batch);
    }

    void FruitSpawnController::setDifficultyTier(int tier) noexcept
    {
        waveController_.setDifficultyTier(tier);
    }

    Fruit::Fruit(const FruitSpriteSet* spriteSet, const IMAGE* shadowSprite, Vec2 position, Vec2 velocity, double gravity, double angularVelocity)
        : spriteSet_(spriteSet), shadowSprite_(shadowSprite), position_(position), velocity_(velocity), gravity_(gravity), angularVelocity_(angularVelocity)
    {
        if (spriteSet_ != nullptr && spriteSet_->whole != nullptr)
        {
            width_ = spriteSet_->whole->getwidth();
            height_ = spriteSet_->whole->getheight();
            radius_ = std::max(width_, height_) * 0.45;
        }
    }

    void Fruit::update(double dt)
    {
        velocity_.y += gravity_ * dt;
        position_.x += velocity_.x * dt;
        position_.y += velocity_.y * dt;
        angle_ += angularVelocity_ * dt;

        if (angle_ > kTwoPi)
        {
            angle_ -= kTwoPi;
        }
        else if (angle_ < -kTwoPi)
        {
            angle_ += kTwoPi;
        }
    }

    void Fruit::drawShadow() const
    {
        if (shadowSprite_ == nullptr || shadowSprite_->getwidth() <= 0 || shadowSprite_->getheight() <= 0)
        {
            return;
        }

        const int shadowX = clampInt(static_cast<int>(std::round(position_.x + width_ * 0.04)), -2000, 2000 + kWindowWidth);
        const int shadowY = clampInt(static_cast<int>(std::round(position_.y + height_ * 0.72 + 8.0)), -2000, 2000 + kWindowHeight);
        drawRotatedImageHQ(*shadowSprite_, shadowX, shadowY, 0.0);
    }

    void Fruit::drawFruit() const
    {
        if (spriteSet_ == nullptr || spriteSet_->whole == nullptr || width_ <= 0 || height_ <= 0)
        {
            return;
        }

        const int centerX = clampInt(static_cast<int>(std::round(position_.x)), -2000, 2000 + kWindowWidth);
        const int centerY = clampInt(static_cast<int>(std::round(position_.y)), -2000, 2000 + kWindowHeight);

        if (isBomb())
        {
            drawBombFlame(position_, angle_);
        }

        drawRotatedFruitImage(*spriteSet_->whole, centerX, centerY, angle_);
    }

    bool Fruit::isOffScreen() const
    {
        return position_.x < -160.0 || position_.x > kWindowWidth + 160.0 || position_.y > kWindowHeight + 220.0;
    }

    RECT Fruit::bounds() const
    {
        const int left = static_cast<int>(std::round(position_.x - radius_));
        const int top = static_cast<int>(std::round(position_.y - radius_));
        const int right = static_cast<int>(std::round(position_.x + radius_));
        const int bottom = static_cast<int>(std::round(position_.y + radius_));
        return RECT{ left, top, right, bottom };
    }

    const Vec2& Fruit::position() const noexcept
    {
        return position_;
    }

    const Vec2& Fruit::velocity() const noexcept
    {
        return velocity_;
    }

    double Fruit::radius() const noexcept
    {
        return radius_;
    }

    double Fruit::angle() const noexcept
    {
        return angle_;
    }

    double Fruit::angularVelocity() const noexcept
    {
        return angularVelocity_;
    }

    bool Fruit::hitTest(const POINT& p, double expandScale) const
    {
        const double r = radius_ * expandScale;
        const double dx = static_cast<double>(p.x) - position_.x;
        const double dy = static_cast<double>(p.y) - position_.y;
        return dx * dx + dy * dy <= r * r;
    }

    const FruitSpriteSet* Fruit::spriteSet() const noexcept
    {
        return spriteSet_;
    }

    bool Fruit::isBomb() const noexcept
    {
        return spriteSet_ != nullptr && spriteSet_->isBomb;
    }

    void FruitField::FruitHalf::update(double dt, double gravity)
    {
        velocity.y += gravity * dt;
        position.x += velocity.x * dt;
        position.y += velocity.y * dt;
        angle += angularVelocity * dt;

        if (angle > kTwoPi)
        {
            angle -= kTwoPi;
        }
        else if (angle < -kTwoPi)
        {
            angle += kTwoPi;
        }
    }

    void FruitField::FruitHalf::draw() const
    {
        if (sprite == nullptr || width <= 0 || height <= 0)
        {
            return;
        }

        const int cx = clampInt(static_cast<int>(std::round(position.x)), -2200, 2200 + kWindowWidth);
        const int cy = clampInt(static_cast<int>(std::round(position.y)), -2200, 2200 + kWindowHeight);
        drawRotatedFruitImage(*sprite, cx, cy, angle);
    }

    bool FruitField::FruitHalf::isOffScreen() const
    {
        return position.x < -220.0 ||
            position.x > kWindowWidth + 220.0 ||
            position.y > kWindowHeight + 260.0;
    }

    FruitField::FruitField(std::vector<const FruitSpriteSet*> sprites,
        const FruitSpriteSet* bombSprite,
        const IMAGE* backgroundSprite,
        const IMAGE* shadowSprite)
        : backgroundSprite_(backgroundSprite), shadowSprite_(shadowSprite), spriteSets_(std::move(sprites)), bombSpriteSet_(bombSprite), rng_(std::random_device{}())
    {
    }

    void FruitField::seedInitialFruits(std::size_t count)
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            spawnOneRandomFruit();
        }
    }

    void FruitField::update(double dt)
    {
        if (spawnEnabled_)
        {
            spawnController_.update(dt);
            SpawnBatch batch;
            while (spawnController_.tryConsumeBatch(batch))
            {
                spawnWaveCount_ += 1;
                if (batch.bomb)
                {
                    if (bombSpriteSet_ != nullptr)
                    {
                        spawnOneBomb();
                        earlyBombWaveUsed_ = true;
                    }
                    else
                    {
                        spawnScatterGroup(1);
                    }
                    continue;
                }

                if (batch.count <= 0)
                {
                    continue;
                }

                if (batch.coherentVertical && batch.count > 1)
                {
                    spawnVerticalGroup(batch.count, batch.sameFruit);
                }
                else if (batch.count == 1)
                {
                    spawnScatterGroup(1);
                }
                else
                {
                    spawnScatterGroup(batch.count);
                }
            }
        }

        for (auto& fruit : fruits_)
        {
            fruit.update(dt);
        }

        for (auto& half : slicedHalves_)
        {
            half.update(dt, kGravity);
        }

        int missedThisFrame = 0;
        fruits_.erase(
            std::remove_if(fruits_.begin(), fruits_.end(), [&missedThisFrame](const Fruit& fruit)
                {
                    if (!fruit.isOffScreen())
                    {
                        return false;
                    }
                    if (!fruit.isBomb())
                    {
                        missedThisFrame += 1;
                    }
                    return true; }),
            fruits_.end());
        missedCount_ += missedThisFrame;

        slicedHalves_.erase(
            std::remove_if(slicedHalves_.begin(), slicedHalves_.end(), [](const FruitHalf& half)
                { return half.isOffScreen(); }),
            slicedHalves_.end());
    }

    void FruitField::render() const
    {
        if (backgroundSprite_ != nullptr && backgroundSprite_->getwidth() > 0 && backgroundSprite_->getheight() > 0)
        {
            putimage(0, 0, backgroundSprite_);
        }
        else
        {
            setfillcolor(RGB(78, 171, 238));
            solidrectangle(0, 0, kWindowWidth, kWindowHeight);
            setfillcolor(RGB(44, 160, 77));
            solidrectangle(0, kWindowHeight - 76, kWindowWidth, kWindowHeight);
        }

        for (const auto& fruit : fruits_)
        {
            fruit.drawShadow();
        }

        for (const auto& fruit : fruits_)
        {
            fruit.drawFruit();
        }

        for (const auto& half : slicedHalves_)
        {
            half.draw();
        }
    }

    int FruitField::trySliceAlongSegment(const POINT& from, const POINT& to, double bladeAngle, std::vector<SliceEvent>* events)
    {
        if (fruits_.empty())
        {
            return 0;
        }

        int slicedCount = 0;
        std::vector<Fruit> survivors;
        survivors.reserve(fruits_.size());

        const Vec2 segA{ static_cast<double>(from.x), static_cast<double>(from.y) };
        const Vec2 segB{ static_cast<double>(to.x), static_cast<double>(to.y) };

        for (const auto& candidate : fruits_)
        {
            const double hitRadius = candidate.radius() * 1.15 + 10.0;
            const double d2 = distanceSqPointToSegment(candidate.position(), segA, segB);
            if (d2 > hitRadius * hitRadius)
            {
                survivors.push_back(candidate);
                continue;
            }

            const Fruit sliced = candidate;

            const FruitSpriteSet* set = sliced.spriteSet();
            if (set == nullptr)
            {
                continue;
            }

            const POINT impact{
                static_cast<LONG>(std::round(sliced.position().x)),
                static_cast<LONG>(std::round(sliced.position().y)) };

            if (sliced.isBomb())
            {
                if (bombHitCallback_)
                {
                    bombHitCallback_(impact);
                }
                continue;
            }

            slicedCount += 1;

            if (events != nullptr)
            {
                SliceEvent event;
                event.impactPoint = impact;
                event.angle = bladeAngle;
                event.juiceColor = set->juiceColor;
                event.cutSfx = set->cutSfx;
                events->push_back(event);
            }

            const Vec2 basePos = sliced.position();
            const Vec2 baseVel = sliced.velocity();
            const double baseAngle = sliced.angle();
            const double baseAV = sliced.angularVelocity();

            const double splitPush = 260.0 + std::min(180.0, std::abs(baseVel.x) * 0.50);
            const double upBoost = -70.0;
            const double sideY = 28.0;
            const double splitX = std::cos(bladeAngle + 1.57079632679);
            const double splitY = std::sin(bladeAngle + 1.57079632679);
            const double inheritX = baseVel.x * 1.05;
            const double inheritY = baseVel.y * 0.92;

            if (set->half1 != nullptr && set->half1->getwidth() > 0 && set->half1->getheight() > 0)
            {
                FruitHalf h1;
                h1.sprite = set->half1;
                h1.position = basePos;
                h1.velocity = Vec2{ inheritX + splitX * splitPush, inheritY + upBoost + splitY * sideY };
                h1.angle = baseAngle;
                h1.angularVelocity = baseAV - 4.2;
                h1.width = set->half1->getwidth();
                h1.height = set->half1->getheight();
                slicedHalves_.push_back(h1);
            }

            if (set->half2 != nullptr && set->half2->getwidth() > 0 && set->half2->getheight() > 0)
            {
                FruitHalf h2;
                h2.sprite = set->half2;
                h2.position = basePos;
                h2.velocity = Vec2{ inheritX - splitX * splitPush, inheritY + upBoost - splitY * sideY };
                h2.angle = baseAngle;
                h2.angularVelocity = baseAV + 4.2;
                h2.width = set->half2->getwidth();
                h2.height = set->half2->getheight();
                slicedHalves_.push_back(h2);
            }
        }

        fruits_.swap(survivors);

        return slicedCount;
    }

    const std::vector<Fruit>& FruitField::fruits() const noexcept
    {
        return fruits_;
    }

    std::vector<Fruit>& FruitField::fruits() noexcept
    {
        return fruits_;
    }

    int FruitField::missedCount() const noexcept
    {
        return missedCount_;
    }

    int FruitField::consumeSpawnedSinceLastFrame() noexcept
    {
        const int value = spawnedSinceLastFrame_;
        spawnedSinceLastFrame_ = 0;
        return value;
    }

    void FruitField::setSpawnCallback(SpawnCallback callback)
    {
        spawnCallback_ = std::move(callback);
    }

    void FruitField::setBombHitCallback(BombHitCallback callback)
    {
        bombHitCallback_ = std::move(callback);
    }

    void FruitField::setSpawnEnabled(bool enabled) noexcept
    {
        spawnEnabled_ = enabled;
    }

    void FruitField::setDifficultyTier(int tier) noexcept
    {
        int mappedTier = 0;
        if (tier >= 45)
        {
            mappedTier = 3;
        }
        else if (tier >= 30)
        {
            mappedTier = 2;
        }
        else if (tier >= 15)
        {
            mappedTier = 1;
        }

        difficultyTier_ = mappedTier;
        spawnController_.setDifficultyTier(difficultyTier_);
    }

    void FruitField::spawnOneRandomFruit()
    {
        if (fruits_.size() >= kMaxFruits || spriteSets_.empty())
        {
            return;
        }

        std::uniform_int_distribution<std::size_t> spriteDist(0, spriteSets_.size() - 1);
        std::uniform_real_distribution<double> xDist(112.0, static_cast<double>(kWindowWidth - 112));
        std::uniform_real_distribution<double> vxDist(-210.0, 210.0);
        std::uniform_real_distribution<double> vyDist(-1260.0, -1040.0);
        std::uniform_real_distribution<double> avDist(-7.4, 7.4);

        const FruitSpriteSet* set = spriteSets_[spriteDist(rng_)];
        if (set == nullptr || set->whole == nullptr || set->whole->getwidth() <= 0 || set->whole->getheight() <= 0)
        {
            return;
        }

        const double speedMul = difficultySpeedMultiplier(difficultyTier_);
        const double startX = clampSpawnX(xDist(rng_));
        const double startY = static_cast<double>(kWindowHeight) + 85.0;
        double launchVX = vxDist(rng_) * (0.94 + 0.015 * difficultyTier_);
        const double launchVY = vyDist(rng_) * speedMul;
        const double angularVelocity = avDist(rng_);

        launchVX = biasVelocityInward(startX, launchVX, rng_);

        spawnedSinceLastFrame_ += 1;
        if (spawnCallback_)
        {
            spawnCallback_(1);
        }
        fruits_.emplace_back(set, shadowSprite_, Vec2{ startX, startY }, Vec2{ launchVX, launchVY }, kGravity, angularVelocity);
    }

    void FruitField::spawnOneBomb()
    {
        if (fruits_.size() >= kMaxFruits || bombSpriteSet_ == nullptr || bombSpriteSet_->whole == nullptr)
        {
            return;
        }

        if (bombSpriteSet_->whole->getwidth() <= 0 || bombSpriteSet_->whole->getheight() <= 0)
        {
            return;
        }

        std::uniform_real_distribution<double> xDist(120.0, static_cast<double>(kWindowWidth - 120));
        std::uniform_real_distribution<double> vxDist(-190.0, 190.0);
        std::uniform_real_distribution<double> vyDist(-1160.0, -980.0);
        std::uniform_real_distribution<double> avDist(-5.2, 5.2);

        const double speedMul = difficultySpeedMultiplier(difficultyTier_);
        const double startX = clampSpawnX(xDist(rng_));
        const double startY = static_cast<double>(kWindowHeight) + 88.0;
        double launchVX = vxDist(rng_) * (0.92 + 0.015 * difficultyTier_);
        const double launchVY = vyDist(rng_) * speedMul;
        const double angularVelocity = avDist(rng_);

        launchVX = biasVelocityInward(startX, launchVX, rng_);

        spawnedSinceLastFrame_ += 1;
        if (spawnCallback_)
        {
            spawnCallback_(1);
        }
        fruits_.emplace_back(bombSpriteSet_, shadowSprite_, Vec2{ startX, startY }, Vec2{ launchVX, launchVY }, kGravity, angularVelocity);
    }

    void FruitField::spawnVerticalGroup(int count, bool sameFruit)
    {
        if (count <= 0)
        {
            return;
        }

        std::uniform_real_distribution<double> centerXDist(210.0, static_cast<double>(kWindowWidth - 210));
        std::uniform_real_distribution<double> baseVxDist(-40.0, 40.0);
        std::uniform_real_distribution<double> baseVyDist(-1200.0, -1040.0);
        std::uniform_real_distribution<double> jitterXDist(-10.0, 10.0);
        std::uniform_real_distribution<double> jitterVxDist(-28.0, 28.0);
        std::uniform_real_distribution<double> jitterVyDist(-34.0, 34.0);
        std::uniform_real_distribution<double> avDist(-7.2, 7.2);
        std::uniform_int_distribution<std::size_t> spriteDist(0, spriteSets_.size() - 1);

        const double speedMul = difficultySpeedMultiplier(difficultyTier_);
        const double centerX = centerXDist(rng_);
        const double baseVx = baseVxDist(rng_) * (0.92 + 0.015 * difficultyTier_);
        const double baseVy = baseVyDist(rng_) * speedMul;
        const FruitSpriteSet* sharedSet = nullptr;
        if (sameFruit)
        {
            sharedSet = spriteSets_[spriteDist(rng_)];
            if (sharedSet == nullptr || sharedSet->whole == nullptr || sharedSet->whole->getwidth() <= 0 || sharedSet->whole->getheight() <= 0)
            {
                sharedSet = nullptr;
            }
        }

        for (int i = 0; i < count; ++i)
        {
            if (fruits_.size() >= kMaxFruits)
            {
                break;
            }

            const double rank = static_cast<double>(i) - static_cast<double>(count - 1) * 0.5;
            const double startX = clampSpawnX(centerX + rank * 58.0 + jitterXDist(rng_));
            const double startY = static_cast<double>(kWindowHeight) + 88.0;

            double launchVX = baseVx + jitterVxDist(rng_);
            const double launchVY = baseVy + jitterVyDist(rng_);

            launchVX = biasVelocityInward(startX, launchVX, rng_);

            const FruitSpriteSet* set = sameFruit && sharedSet != nullptr ? sharedSet : spriteSets_[spriteDist(rng_)];
            if (set == nullptr || set->whole == nullptr || set->whole->getwidth() <= 0 || set->whole->getheight() <= 0)
            {
                continue;
            }

            spawnedSinceLastFrame_ += 1;
            if (spawnCallback_)
            {
                spawnCallback_(1);
            }
            fruits_.emplace_back(set, shadowSprite_, Vec2{ startX, startY }, Vec2{ launchVX, launchVY }, kGravity, avDist(rng_));
        }
    }

    void FruitField::spawnScatterGroup(int count)
    {
        for (int i = 0; i < count; ++i)
        {
            spawnOneRandomFruit();
        }
    }
}
