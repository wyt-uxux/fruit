#define NOMINMAX
#include "SlashEffects.h"

#include "Fruit.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

namespace FruitGame
{
    namespace
    {
        constexpr double kPi = 3.14159265358979323846;

        inline double clamp01(double value)
        {
            return std::max(0.0, std::min(1.0, value));
        }

        COLORREF blendJuiceColor(COLORREF color, double t)
        {
            const int r = static_cast<int>(GetRValue(color));
            const int g = static_cast<int>(GetGValue(color));
            const int b = static_cast<int>(GetBValue(color));
            const int targetR = 255;
            const int targetG = 255;
            const int targetB = 255;
            const int rr = static_cast<int>(std::round(r + (targetR - r) * t));
            const int gg = static_cast<int>(std::round(g + (targetG - g) * t));
            const int bb = static_cast<int>(std::round(b + (targetB - b) * t));
            return RGB(rr, gg, bb);
        }

        POINT makePoint(double x, double y)
        {
            POINT p{};
            p.x = static_cast<LONG>(std::round(x));
            p.y = static_cast<LONG>(std::round(y));
            return p;
        }

        void drawBeamLayer(int cx, int cy, double angle, double halfLength, double halfWidth, COLORREF color)
        {
            const double vx = std::cos(angle);
            const double vy = std::sin(angle);
            const double nx = -vy;
            const double ny = vx;

            POINT quad[4] = {
                makePoint(cx - vx * halfLength, cy - vy * halfLength),
                makePoint(cx - nx * halfWidth, cy - ny * halfWidth),
                makePoint(cx + vx * halfLength, cy + vy * halfLength),
                makePoint(cx + nx * halfWidth, cy + ny * halfWidth) };

            setfillcolor(color);
            solidpolygon(quad, 4);
        }

        void drawProceduralFlash(int centerX, int centerY, double angle, double progress)
        {
            const double pulse = std::sin(progress * kPi);
            const double stretch = 0.90 + 0.32 * clamp01(pulse);
            const double halfLen = 140.0 * stretch;

            drawBeamLayer(centerX, centerY, angle, halfLen * 1.06, 10.5, RGB(196, 148, 44));
            drawBeamLayer(centerX, centerY, angle, halfLen * 0.96, 6.0, RGB(240, 210, 116));
            drawBeamLayer(centerX, centerY, angle, halfLen * 0.84, 3.0, RGB(255, 252, 234));

            const int lineLen = static_cast<int>(std::round(halfLen * 1.03));
            const int x1 = centerX - static_cast<int>(std::round(std::cos(angle) * lineLen));
            const int y1 = centerY - static_cast<int>(std::round(std::sin(angle) * lineLen));
            const int x2 = centerX + static_cast<int>(std::round(std::cos(angle) * lineLen));
            const int y2 = centerY + static_cast<int>(std::round(std::sin(angle) * lineLen));
            setlinecolor(RGB(255, 247, 212));
            setlinestyle(PS_SOLID, 2);
            line(x1, y1, x2, y2);
        }
    }

    class FlashEffect final : public Effect
    {
    public:
        FlashEffect(const IMAGE* sprite, POINT impactPoint, double angle)
            : impactPoint_(impactPoint), angle_(angle)
        {
            (void)sprite;
        }

        void update(double dt) override
        {
            timer_ += dt;
        }

        void render() const override
        {
            if (!isAlive())
            {
                return;
            }

            const double progress = clamp01(timer_ / kDuration);
            drawProceduralFlash(impactPoint_.x, impactPoint_.y, angle_, progress);
        }

        bool isAlive() const override
        {
            return timer_ < kDuration;
        }

    private:
        POINT impactPoint_{};
        double angle_ = 0.0;
        double timer_ = 0.0;
        static constexpr double kDuration = 0.10;
    };

    class JuiceEffect final : public Effect
    {
    public:
        JuiceEffect(POINT impactPoint, COLORREF color)
            : impactPoint_(impactPoint), baseColor_(color), rng_(std::random_device{}())
        {
            std::uniform_real_distribution<double> angleDist(0.0, kPi * 2.0);
            std::uniform_real_distribution<double> speedDist(100.0, 260.0);
            std::uniform_real_distribution<double> lifeDist(0.60, 1.00);
            std::uniform_real_distribution<double> radiusDist(3.6, 7.4);

            particles_.reserve(12);
            for (int i = 0; i < 12; ++i)
            {
                const double angle = angleDist(rng_);
                const double speed = speedDist(rng_);
                Particle particle;
                particle.position.x = static_cast<double>(impactPoint_.x);
                particle.position.y = static_cast<double>(impactPoint_.y);
                particle.velocity.x = std::cos(angle) * speed;
                particle.velocity.y = std::sin(angle) * speed - 50.0;
                particle.life = lifeDist(rng_);
                particle.maxLife = particle.life;
                particle.radius = radiusDist(rng_);
                particles_.push_back(particle);
            }
        }

        void update(double dt) override
        {
            for (auto& particle : particles_)
            {
                if (particle.life <= 0.0)
                {
                    continue;
                }

                particle.life -= dt;
                particle.velocity.y += 680.0 * dt;
                particle.velocity.x *= 0.992;
                particle.velocity.y *= 0.996;
                particle.position.x += particle.velocity.x * dt;
                particle.position.y += particle.velocity.y * dt;
            }
        }

        void render() const override
        {
            for (const auto& particle : particles_)
            {
                if (particle.life <= 0.0)
                {
                    continue;
                }

                const double lifeRatio = clamp01(particle.life / particle.maxLife);
                const double fade = 1.0 - lifeRatio;
                const double radius = std::max(1.0, particle.radius * (0.65 + 0.75 * lifeRatio));
                const COLORREF color = blendJuiceColor(baseColor_, fade * 0.55);

                setfillcolor(color);
                solidcircle(static_cast<int>(std::round(particle.position.x)), static_cast<int>(std::round(particle.position.y)), static_cast<int>(std::round(radius)));
            }
        }

        bool isAlive() const override
        {
            for (const auto& particle : particles_)
            {
                if (particle.life > 0.0)
                {
                    return true;
                }
            }
            return false;
        }

    private:
        struct Particle
        {
            Vec2 position{};
            Vec2 velocity{};
            double life = 0.0;
            double maxLife = 0.0;
            double radius = 0.0;
        };

        POINT impactPoint_{};
        COLORREF baseColor_ = RGB(255, 255, 255);
        std::vector<Particle> particles_;
        std::mt19937 rng_;
    };

    std::unique_ptr<Effect> makeFlashEffect(const IMAGE* flashSprite, POINT impactPoint, double angle)
    {
        return std::make_unique<FlashEffect>(flashSprite, impactPoint, angle);
    }

    std::unique_ptr<Effect> makeJuiceEffect(POINT impactPoint, COLORREF color)
    {
        return std::make_unique<JuiceEffect>(impactPoint, color);
    }
}
