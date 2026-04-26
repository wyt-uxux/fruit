#pragma once

#include <filesystem>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <condition_variable>

namespace FruitGame
{
    enum class UIScene
    {
        MainMenu,
        Gameplay,
        GameOver,
        KnifeSelect
    };

    enum class SfxId
    {
        Start,
        Throw,
        Boom,
        Splatter,
        Splatter2,
        SplatterBanana,
        Combo3,
        Combo4,
        Error,
        Knife1,
        Knife2,
        Knife3,
        Knife4
    };

    class MusicController
    {
    public:
        MusicController();
        ~MusicController();

        MusicController(const MusicController&) = delete;
        MusicController& operator=(const MusicController&) = delete;

        void registerSceneBgm(UIScene scene, const std::filesystem::path& filePath, bool loop = true);
        void registerSfx(SfxId id, const std::filesystem::path& filePath, int channels = 4);

        void enterScene(UIScene scene);
        void playSfx(SfxId id);

        void stopBgm();
        void stopAll();

    private:
        struct BgmTrack
        {
            std::filesystem::path filePath;
            bool loop = true;
        };

        struct SfxPool
        {
            std::filesystem::path filePath;
            std::vector<std::wstring> aliases;
            std::vector<unsigned char> opened;
            std::size_t nextIndex = 0;
        };

        static bool sendCommand(const std::wstring& cmd);
        static std::wstring quotePath(const std::filesystem::path& path);
        static std::wstring mediaTypeFor(const std::filesystem::path& path);

        bool openAlias(const std::wstring& alias, const std::filesystem::path& filePath) const;
        void closeAlias(const std::wstring& alias) const;
        void playSfxImmediate(SfxId id);
        void startSfxWorker();
        void stopSfxWorker();
        void sfxWorkerLoop();

        std::unordered_map<UIScene, BgmTrack> sceneBgm_;
        std::unordered_map<SfxId, SfxPool> sfxPools_;
        mutable std::mutex audioMutex_;
        std::mutex sfxQueueMutex_;
        std::condition_variable sfxQueueCv_;
        std::deque<SfxId> sfxQueue_;
        std::thread sfxWorker_;
        bool sfxWorkerRunning_ = false;
        bool sfxWorkerStop_ = false;
        UIScene currentScene_ = UIScene::Gameplay;
        bool hasScene_ = false;
        std::wstring bgmAlias_ = L"bgm_main";
        bool bgmOpen_ = false;
    };
}

