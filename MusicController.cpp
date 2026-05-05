#define NOMINMAX
#include "MusicController.h"

#include <Windows.h>
#include <mmsystem.h>

#include <algorithm>

#pragma comment(lib, "winmm.lib")

namespace FruitGame
{
    namespace
    {
        constexpr bool kEnableAsyncSfxWorker = true;
        constexpr std::size_t kMaxPendingSfx = 96;

        std::wstring toLower(std::wstring v)
        {
            std::transform(v.begin(), v.end(), v.begin(), towlower);
            return v;
        }
    }

    MusicController::MusicController()
    {
        startSfxWorker();
    }

    MusicController::~MusicController()
    {
        stopAll();
    }

    void MusicController::registerSceneBgm(UIScene scene, const std::filesystem::path& filePath, bool loop)
    {
        const std::scoped_lock<std::mutex> lock(audioMutex_);
        if (filePath.empty() || !std::filesystem::exists(filePath))
        {
            return;
        }

        sceneBgm_[scene] = BgmTrack{ filePath, loop };
    }

    void MusicController::registerSfx(SfxId id, const std::filesystem::path& filePath, int channels)
    {
        const std::scoped_lock<std::mutex> lock(audioMutex_);
        if (filePath.empty() || !std::filesystem::exists(filePath))
        {
            return;
        }

        auto existing = sfxPools_.find(id);
        if (existing != sfxPools_.end())
        {
            for (std::size_t i = 0; i < existing->second.aliases.size(); ++i)
            {
                if (i < existing->second.opened.size() && existing->second.opened[i])
                {
                    sendCommand(L"stop " + existing->second.aliases[i]);
                    closeAlias(existing->second.aliases[i]);
                }
            }
        }

        const int channelCount = std::max(1, channels);
        SfxPool pool;
        pool.filePath = filePath;
        pool.aliases.reserve(static_cast<std::size_t>(channelCount));
        pool.opened.reserve(static_cast<std::size_t>(channelCount));

        const std::wstring idPart = std::to_wstring(static_cast<int>(id));
        for (int i = 0; i < channelCount; ++i)
        {
            const std::wstring alias = L"sfx_" + idPart + L"_" + std::to_wstring(i);
            pool.aliases.push_back(alias);
            pool.opened.push_back(openAlias(alias, filePath) ? 1 : 0);
        }

        sfxPools_[id] = std::move(pool);
    }

    void MusicController::enterScene(UIScene scene)
    {
        currentScene_ = scene;
        hasScene_ = true;

        stopBgm();

        const std::scoped_lock<std::mutex> lock(audioMutex_);

        const auto it = sceneBgm_.find(scene);
        if (it == sceneBgm_.end())
        {
            return;
        }

        const BgmTrack& track = it->second;
        if (!openAlias(bgmAlias_, track.filePath))
        {
            return;
        }

        bgmOpen_ = true;
        if (track.loop)
        {
            sendCommand(L"play " + bgmAlias_ + L" repeat");
        }
        else
        {
            sendCommand(L"play " + bgmAlias_ + L" from 0");
        }
    }

    void MusicController::playSfx(SfxId id)
    {
        if (kEnableAsyncSfxWorker && sfxWorkerRunning_)
        {
            {
                const std::scoped_lock<std::mutex> qlock(sfxQueueMutex_);
                if (sfxQueue_.size() >= kMaxPendingSfx)
                {
                    sfxQueue_.pop_front();
                }
                sfxQueue_.push_back(id);
            }
            sfxQueueCv_.notify_one();
            return;
        }

        playSfxImmediate(id);
    }

    void MusicController::playSfxImmediate(SfxId id)
    {
        const std::scoped_lock<std::mutex> lock(audioMutex_);
        const auto it = sfxPools_.find(id);
        if (it == sfxPools_.end())
        {
            return;
        }

        SfxPool& pool = it->second;
        if (pool.aliases.empty())
        {
            return;
        }

        const std::wstring& alias = pool.aliases[pool.nextIndex];
        unsigned char& opened = pool.opened[pool.nextIndex];
        pool.nextIndex = (pool.nextIndex + 1) % pool.aliases.size();

        if (!opened)
        {
            opened = openAlias(alias, pool.filePath) ? 1 : 0;
            if (!opened)
            {
                return;
            }
        }

        const bool okStop = sendCommand(L"stop " + alias);
        const bool okSeek = sendCommand(L"seek " + alias + L" to start");
        const bool okPlay = sendCommand(L"play " + alias + L" from 0");
        if (okStop && okSeek && okPlay)
        {
            return;
        }

        closeAlias(alias);
        opened = openAlias(alias, pool.filePath) ? 1 : 0;
        if (!opened)
        {
            return;
        }

        sendCommand(L"stop " + alias);
        sendCommand(L"seek " + alias + L" to start");
        sendCommand(L"play " + alias + L" from 0");
    }

    void MusicController::stopBgm()
    {
        const std::scoped_lock<std::mutex> lock(audioMutex_);
        if (!bgmOpen_)
        {
            return;
        }

        sendCommand(L"stop " + bgmAlias_);
        closeAlias(bgmAlias_);
        bgmOpen_ = false;
    }

    void MusicController::stopAll()
    {
        stopSfxWorker();
        stopBgm();

        const std::scoped_lock<std::mutex> lock(audioMutex_);

        for (const auto& pair : sfxPools_)
        {
            const SfxPool& pool = pair.second;
            for (std::size_t i = 0; i < pool.aliases.size(); ++i)
            {
                const std::wstring& alias = pool.aliases[i];
                sendCommand(L"stop " + alias);
                if (i < pool.opened.size() && pool.opened[i])
                {
                    closeAlias(alias);
                }
            }
        }
    }

    void MusicController::startSfxWorker()
    {
        if (!kEnableAsyncSfxWorker)
        {
            return;
        }

        const std::scoped_lock<std::mutex> lock(sfxQueueMutex_);
        if (sfxWorkerRunning_)
        {
            return;
        }

        sfxWorkerStop_ = false;
        sfxWorker_ = std::thread(&MusicController::sfxWorkerLoop, this);
        sfxWorkerRunning_ = true;
    }

    void MusicController::stopSfxWorker()
    {
        if (!kEnableAsyncSfxWorker)
        {
            return;
        }

        {
            const std::scoped_lock<std::mutex> lock(sfxQueueMutex_);
            if (!sfxWorkerRunning_)
            {
                return;
            }
            sfxWorkerStop_ = true;
        }

        sfxQueueCv_.notify_all();

        if (sfxWorker_.joinable())
        {
            sfxWorker_.join();
        }

        {
            const std::scoped_lock<std::mutex> lock(sfxQueueMutex_);
            sfxQueue_.clear();
            sfxWorkerRunning_ = false;
            sfxWorkerStop_ = false;
        }
    }

    void MusicController::sfxWorkerLoop()
    {
        while (true)
        {
            SfxId nextId = SfxId::Start;

            {
                std::unique_lock<std::mutex> lock(sfxQueueMutex_);
                sfxQueueCv_.wait(lock, [this]
                    { return sfxWorkerStop_ || !sfxQueue_.empty(); });

                if (sfxWorkerStop_ && sfxQueue_.empty())
                {
                    break;
                }

                nextId = sfxQueue_.front();
                sfxQueue_.pop_front();
            }

            playSfxImmediate(nextId);
        }
    }

    bool MusicController::sendCommand(const std::wstring& cmd)
    {
        return mciSendStringW(cmd.c_str(), nullptr, 0, nullptr) == 0;
    }

    std::wstring MusicController::quotePath(const std::filesystem::path& path)
    {
        return L"\"" + path.wstring() + L"\"";
    }

    std::wstring MusicController::mediaTypeFor(const std::filesystem::path& path)
    {
        const std::wstring ext = toLower(path.extension().wstring());
        if (ext == L".wav")
        {
            return L"waveaudio";
        }

        return L"mpegvideo";
    }

    bool MusicController::openAlias(const std::wstring& alias, const std::filesystem::path& filePath) const
    {
        closeAlias(alias);

        const std::wstring cmd = L"open " + quotePath(filePath) + L" type " + mediaTypeFor(filePath) + L" alias " + alias;
        return sendCommand(cmd);
    }

    void MusicController::closeAlias(const std::wstring& alias) const
    {
        sendCommand(L"close " + alias);
    }
}
