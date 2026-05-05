#define NOMINMAX

#include "KnifeEffect.h"

#include "Fruit.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <random>

#include <graphics.h>

namespace FruitGame
{
    namespace
    {
        constexpr double kTwoPi = 6.28318530717958647692;

        inline double clamp01(double value)
        {
            return std::max(0.0, std::min(1.0, value));
        }

        inline int lerpInt(int a, int b, double t)
        {
            return static_cast<int>(std::lround(a + (b - a) * clamp01(t)));
        }

        inline double smoothstep(double t)
        {
            t = clamp01(t);
            return t * t * (3.0 - 2.0 * t);
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

        void brightenImage(IMAGE& img)
        {
            if (img.getwidth() <= 0 || img.getheight() <= 0)
            {
                return;
            }

            DWORD* buf = GetImageBuffer(&img);
            if (buf == nullptr)
            {
                return;
            }

            const int pixels = img.getwidth() * img.getheight();
            for (int i = 0; i < pixels; ++i)
            {
                DWORD px = buf[i];
                const unsigned char a = getA(px);
                if (a == 0)
                {
                    continue;
                }

                const int r = std::min(255, static_cast<int>(std::round(getR(px) * 1.30 + 18.0)));
                const int g = std::min(255, static_cast<int>(std::round(getG(px) * 1.35 + 22.0)));
                const int b = std::min(255, static_cast<int>(std::round(getB(px) * 1.18 + 10.0)));
                buf[i] = makeARGB(a, static_cast<unsigned char>(r), static_cast<unsigned char>(g), static_cast<unsigned char>(b));
            }
        }

        void cleanupRotatedBlackMatte(IMAGE& img)
        {
            if (img.getwidth() <= 0 || img.getheight() <= 0)
            {
                return;
            }

            DWORD* buf = GetImageBuffer(&img);
            if (buf == nullptr)
            {
                return;
            }

            const int pixels = img.getwidth() * img.getheight();
            for (int i = 0; i < pixels; ++i)
            {
                DWORD px = buf[i];
                const int r = static_cast<int>(getR(px));
                const int g = static_cast<int>(getG(px));
                const int b = static_cast<int>(getB(px));

                // rotateimage can introduce black/near-black matte around transparent edges.
                if ((r <= 20 && g <= 20 && b <= 20) || (r + g + b <= 46))
                {
                    buf[i] = makeARGB(0, 0, 0, 0);
                    continue;
                }

                const unsigned char a = getA(px);
                buf[i] = makeARGB(std::max<unsigned char>(a, 255), getR(px), getG(px), getB(px));
            }
        }

        void drawImageAlphaScaled(const IMAGE* img, int dstX, int dstY, double scale, double alphaMul)
        {
            if (img == nullptr || img->getwidth() <= 0 || img->getheight() <= 0)
            {
                return;
            }

            DWORD* dstBuf = GetImageBuffer();
            DWORD* srcBuf = GetImageBuffer(const_cast<IMAGE*>(img));
            if (dstBuf == nullptr || srcBuf == nullptr)
            {
                return;
            }

            const int dstW = getwidth();
            const int dstH = getheight();
            const int srcW = img->getwidth();
            const int srcH = img->getheight();
            const int w = std::max(1, static_cast<int>(std::round(srcW * scale)));
            const int h = std::max(1, static_cast<int>(std::round(srcH * scale)));

            for (int y = 0; y < h; ++y)
            {
                const int sy = std::min(srcH - 1, static_cast<int>(y / scale));
                const int py = dstY + y;
                if (py < 0 || py >= dstH)
                {
                    continue;
                }

                for (int x = 0; x < w; ++x)
                {
                    const int sx = std::min(srcW - 1, static_cast<int>(x / scale));
                    const int px = dstX + x;
                    if (px < 0 || px >= dstW)
                    {
                        continue;
                    }

                    const DWORD srcPx = srcBuf[sy * srcW + sx];
                    const int a = static_cast<int>(std::round(getA(srcPx) * clamp01(alphaMul)));
                    if (a <= 0)
                    {
                        continue;
                    }

                    DWORD& dstPx = dstBuf[py * dstW + px];
                    if (a >= 255)
                    {
                        dstPx = makeARGB(0xFF, getR(srcPx), getG(srcPx), getB(srcPx));
                        continue;
                    }

                    const int ia = 255 - a;
                    const unsigned char rr = static_cast<unsigned char>((getR(srcPx) * a + getR(dstPx) * ia) / 255);
                    const unsigned char gg = static_cast<unsigned char>((getG(srcPx) * a + getG(dstPx) * ia) / 255);
                    const unsigned char bb = static_cast<unsigned char>((getB(srcPx) * a + getB(dstPx) * ia) / 255);
                    dstPx = makeARGB(0xFF, rr, gg, bb);
                }
            }
        }

        void drawRibbonSegment(const POINT& a, const POINT& b, double halfWidth, COLORREF color)
        {
            const double dx = static_cast<double>(b.x - a.x);
            const double dy = static_cast<double>(b.y - a.y);
            const double len = std::sqrt(dx * dx + dy * dy);
            if (len <= 0.001 || halfWidth <= 0.05)
            {
                return;
            }

            const double nx = -dy / len;
            const double ny = dx / len;

            POINT quad[4] = {
                POINT{static_cast<LONG>(std::lround(a.x + nx * halfWidth)), static_cast<LONG>(std::lround(a.y + ny * halfWidth))},
                POINT{static_cast<LONG>(std::lround(a.x - nx * halfWidth)), static_cast<LONG>(std::lround(a.y - ny * halfWidth))},
                POINT{static_cast<LONG>(std::lround(b.x - nx * halfWidth)), static_cast<LONG>(std::lround(b.y - ny * halfWidth))},
                POINT{static_cast<LONG>(std::lround(b.x + nx * halfWidth)), static_cast<LONG>(std::lround(b.y + ny * halfWidth))} };

            setfillcolor(color);
            solidpolygon(quad, 4);
        }

        std::vector<POINT> chaikinSmooth(const std::vector<POINT>& points, int iterations)
        {
            if (points.size() < 3 || iterations <= 0)
            {
                return points;
            }

            auto makePt = [](double x, double y)
                {
                    POINT p{};
                    p.x = static_cast<LONG>(std::lround(x));
                    p.y = static_cast<LONG>(std::lround(y));
                    return p;
                };

            std::vector<POINT> current = points;
            for (int pass = 0; pass < iterations; ++pass)
            {
                if (current.size() < 3)
                {
                    break;
                }

                std::vector<POINT> next;
                next.reserve(current.size() * 2);
                next.push_back(current.front());

                for (std::size_t i = 0; i + 1 < current.size(); ++i)
                {
                    const POINT& a = current[i];
                    const POINT& b = current[i + 1];
                    next.push_back(makePt(0.75 * static_cast<double>(a.x) + 0.25 * static_cast<double>(b.x),
                        0.75 * static_cast<double>(a.y) + 0.25 * static_cast<double>(b.y)));
                    next.push_back(makePt(0.25 * static_cast<double>(a.x) + 0.75 * static_cast<double>(b.x),
                        0.25 * static_cast<double>(a.y) + 0.75 * static_cast<double>(b.y)));
                }

                next.push_back(current.back());
                current = std::move(next);
            }

            return current;
        }

        int rotationFrameIndex(double angle, int frameCount)
        {
            if (frameCount <= 0)
            {
                return 0;
            }

            double a = std::fmod(angle, kTwoPi);
            if (a < 0.0)
            {
                a += kTwoPi;
            }

            int idx = static_cast<int>(std::floor((a / kTwoPi) * static_cast<double>(frameCount)));
            if (idx >= frameCount)
            {
                idx = frameCount - 1;
            }
            return std::max(0, idx);
        }

        void buildRotationFrames(const IMAGE& source, std::vector<std::unique_ptr<IMAGE>>& frames, int frameCount)
        {
            frames.clear();
            if (source.getwidth() <= 0 || source.getheight() <= 0 || frameCount <= 0)
            {
                return;
            }

            frames.reserve(static_cast<std::size_t>(frameCount));
            for (int i = 0; i < frameCount; ++i)
            {
                const double angle = (kTwoPi * static_cast<double>(i)) / static_cast<double>(frameCount);
                auto frame = std::make_unique<IMAGE>();
                rotateimage(frame.get(), const_cast<IMAGE*>(&source), angle, BLACK, true, true);
                cleanupRotatedBlackMatte(*frame);
                frames.push_back(std::move(frame));
            }
        }

        class BambooKnifeEffect final : public KnifeEffect
        {
        public:
            BambooKnifeEffect()
                : rng_(std::random_device{}())
            {
                loadLeaf(leafA_, std::filesystem::absolute("images\\knifeeffect\\bamboo\\bamboo1.png"));
                loadLeaf(leafB_, std::filesystem::absolute("images\\knifeeffect\\bamboo\\bamboo2.png"));
                brightenImage(leafA_);
                brightenImage(leafB_);
                buildRotationFrames(leafA_, leafFramesA_, kRotationFrameCount);
                buildRotationFrames(leafB_, leafFramesB_, kRotationFrameCount);
            }

            void update(double dt, bool slicing, bool hasCursor, const POINT& cursor) override
            {
                if (slicing && hasCursor)
                {
                    spawnTimer_ += dt;
                    while (spawnTimer_ >= kLeafSpawnInterval)
                    {
                        spawnTimer_ -= kLeafSpawnInterval;
                        spawnLeaf(cursor);
                    }
                }
                else
                {
                    spawnTimer_ = 0.0;
                }

                for (auto& leaf : leaves_)
                {
                    if (leaf.life <= 0.0)
                    {
                        continue;
                    }

                    leaf.life -= dt;
                    leaf.velocity.y += 145.0 * dt;
                    leaf.velocity.x *= 0.995;
                    const double age = leaf.maxLife - leaf.life;
                    const double sway = std::sin(age * leaf.swayFreq + leaf.swayPhase) * leaf.swayAmp;
                    leaf.position.x += (leaf.velocity.x + sway) * dt;
                    leaf.position.y += leaf.velocity.y * dt;
                    leaf.angle += leaf.angularVelocity * dt;
                    if (leaf.angle > kTwoPi)
                    {
                        leaf.angle -= kTwoPi;
                    }
                    else if (leaf.angle < -kTwoPi)
                    {
                        leaf.angle += kTwoPi;
                    }
                }

                leaves_.erase(
                    std::remove_if(leaves_.begin(), leaves_.end(), [](const LeafParticle& leaf)
                        { return leaf.life <= 0.0 || leaf.position.y > kWindowHeight + 60.0; }),
                    leaves_.end());
            }

            void renderTrail(const std::vector<POINT>& smoothTrail) const override
            {
                const std::vector<POINT> ribbonTrail = chaikinSmooth(smoothTrail, 4);
                if (ribbonTrail.size() >= 2)
                {
                    const std::size_t step = 1;
                    for (std::size_t i = step; i < ribbonTrail.size(); i += step)
                    {
                        const double t = static_cast<double>(i) / static_cast<double>(ribbonTrail.size() - 1);
                        const double w = 1.6 + 5.6 * smoothstep(t);
                        const double halfWidth = std::max(0.62, w * 0.50);

                        const int bladeR = lerpInt(56, 74, t);
                        const int bladeG = lerpInt(190, 214, t);
                        const int bladeB = lerpInt(172, 192, t);

                        const POINT& a = ribbonTrail[i - step];
                        const POINT& p = ribbonTrail[i];

                        drawRibbonSegment(a, p, halfWidth, RGB(bladeR, bladeG, bladeB));
                    }

                    const POINT& tip = ribbonTrail.back();
                    setfillcolor(RGB(126, 232, 214));
                    solidcircle(tip.x, tip.y, 2);
                    solidcircle(tip.x, tip.y, 1);
                }

                for (const auto& leaf : leaves_)
                {
                    if (leaf.life <= 0.0)
                    {
                        continue;
                    }

                    const IMAGE* img = leaf.imageIndex == 0 ? &leafA_ : &leafB_;
                    const std::vector<std::unique_ptr<IMAGE>>& frames = (leaf.imageIndex == 0) ? leafFramesA_ : leafFramesB_;
                    if (img->getwidth() <= 0 || img->getheight() <= 0)
                    {
                        continue;
                    }

                    const double lifeRatio = clamp01(leaf.life / leaf.maxLife);
                    // Keep visibility in early life, then fade out quickly near the end.
                    const double tailFade = clamp01((0.45 - lifeRatio) / 0.45);
                    const double alphaBase = 0.22 + 0.78 * lifeRatio;
                    const double alpha = alphaBase * (1.0 - tailFade * tailFade);

                    const IMAGE* drawImg = img;
                    if (!frames.empty())
                    {
                        drawImg = frames[rotationFrameIndex(leaf.angle, static_cast<int>(frames.size()))].get();
                    }

                    const int rw = drawImg->getwidth();
                    const int rh = drawImg->getheight();
                    if (rw <= 0 || rh <= 0)
                    {
                        continue;
                    }

                    const int drawX = static_cast<int>(std::round(leaf.position.x - (rw * leaf.scale) * 0.5));
                    const int drawY = static_cast<int>(std::round(leaf.position.y - (rh * leaf.scale) * 0.5));
                    drawImageAlphaScaled(drawImg, drawX, drawY, leaf.scale, alpha);
                }
            }

        private:
            struct LeafParticle
            {
                Vec2 position{};
                Vec2 velocity{};
                double life = 0.0;
                double maxLife = 1.0;
                int imageIndex = 0;
                double scale = 1.0;
                double swayAmp = 0.0;
                double swayFreq = 0.0;
                double swayPhase = 0.0;
                double angle = 0.0;
                double angularVelocity = 0.0;
            };

            void loadLeaf(IMAGE& img, const std::filesystem::path& path)
            {
                if (!std::filesystem::exists(path))
                {
                    return;
                }

                loadimage(&img, path.c_str());
            }

            void spawnLeaf(const POINT& cursor)
            {
                if (leafA_.getwidth() <= 0 && leafB_.getwidth() <= 0)
                {
                    return;
                }

                if (leaves_.size() >= kMaxLeafParticles)
                {
                    return;
                }

                std::uniform_real_distribution<double> xDist(-24.0, 24.0);
                std::uniform_real_distribution<double> yDist(-20.0, 8.0);
                std::uniform_real_distribution<double> angleDist(0.0, 6.28318530717958647692);
                std::uniform_real_distribution<double> speedDist(88.0, 176.0);
                std::uniform_real_distribution<double> lifeDist(0.55, 1.15);
                std::uniform_real_distribution<double> scaleDist(0.08, 0.18);
                std::uniform_real_distribution<double> swayAmpDist(9.0, 23.0);
                std::uniform_real_distribution<double> swayFreqDist(4.5, 7.8);
                std::uniform_real_distribution<double> swayPhaseDist(0.0, 6.28318530717958647692);
                std::uniform_real_distribution<double> startAngleDist(-1.1, 1.1);
                std::uniform_real_distribution<double> spinDist(-6.8, 6.8);
                std::uniform_int_distribution<int> imageDist(0, 1);

                LeafParticle p;
                p.position.x = static_cast<double>(cursor.x) + xDist(rng_);
                p.position.y = static_cast<double>(cursor.y) + yDist(rng_);
                const double angle = angleDist(rng_);
                const double speed = speedDist(rng_);
                p.velocity.x = std::cos(angle) * speed;
                p.velocity.y = std::sin(angle) * speed;
                p.life = lifeDist(rng_);
                p.maxLife = p.life;
                p.imageIndex = imageDist(rng_);
                p.scale = scaleDist(rng_);
                p.swayAmp = swayAmpDist(rng_);
                p.swayFreq = swayFreqDist(rng_);
                p.swayPhase = swayPhaseDist(rng_);
                p.angle = startAngleDist(rng_);
                p.angularVelocity = spinDist(rng_);
                leaves_.push_back(p);
            }

            static constexpr double kLeafSpawnInterval = 0.062;
            static constexpr std::size_t kMaxLeafParticles = 40;
            static constexpr int kRotationFrameCount = 24;
            IMAGE leafA_;
            IMAGE leafB_;
            std::vector<std::unique_ptr<IMAGE>> leafFramesA_;
            std::vector<std::unique_ptr<IMAGE>> leafFramesB_;
            mutable std::vector<LeafParticle> leaves_;
            double spawnTimer_ = 0.0;
            std::mt19937 rng_;
        };
    }

    std::unique_ptr<KnifeEffect> createBambooKnifeEffect()
    {
        return std::make_unique<BambooKnifeEffect>();
    }
}
