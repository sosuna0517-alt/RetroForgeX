#pragma once
// RomLibrary: discovers installed cores (package Cores\ folder) and games on
// local and external (USB 3.0) storage.
//
// Game sources, in order:
//   1. LocalState\roms
//   2. Folders the user picked with FolderPicker (persisted in FutureAccessList)
//   3. <drive>:\roms on D: through K: (needs broadFileSystemAccess granted in
//      Settings > Privacy > File system; silently skipped otherwise)
//   4. KnownFolders::RemovableDevices -> <drive>\roms (removableStorage capability)
//
// Scans block on WinRT async calls, so they must run on an MTA/background thread.

namespace rfx
{
    struct CoreEntry
    {
        std::wstring packagePath;   // e.g. L"Cores\\snes9x_libretro.dll"
        std::wstring fileName;      // e.g. L"snes9x_libretro.dll"
        std::string name;           // library_name
        std::string version;
        std::vector<std::string> extensions; // lowercase, no dot
        bool needFullPath = false;
    };

    struct GameEntry
    {
        std::wstring path;
        std::string displayName;    // UTF-8
        std::string extension;      // lowercase, no dot
        std::vector<int> coreIndices;
        winrt::Windows::Storage::StorageFile file{ nullptr };
    };

    class RomLibrary
    {
    public:
        void ScanCores();  // blocking, background thread
        void ScanGames();  // blocking, background thread
        void AddFolder(winrt::Windows::Storage::StorageFolder const& folder); // any thread

        std::vector<CoreEntry> Cores() const;
        std::vector<GameEntry> Games() const;
        std::vector<std::wstring> SourceFolders() const;
        uint64_t Revision() const { return m_revision.load(); }
        bool IsScanning() const { return m_scanning.load(); }

        // Reads a whole file through the Storage APIs (blocking, background thread).
        static bool ReadAllBytes(winrt::Windows::Storage::StorageFile const& file, std::vector<uint8_t>& out);
        // Returns a path the core's own fopen can open: the original path if the
        // process can open it directly, otherwise a copy under LocalState\cache.
        static std::wstring EnsureDirectlyReadable(winrt::Windows::Storage::StorageFile const& file);

    private:
        void ScanFolder(winrt::Windows::Storage::StorageFolder const& folder,
            std::vector<GameEntry>& out, std::map<std::wstring, bool>& seen,
            std::vector<CoreEntry> const& cores, std::vector<winrt::hstring> const& filter);

        mutable std::mutex m_mutex;
        std::vector<CoreEntry> m_cores;
        std::vector<GameEntry> m_games;
        std::vector<std::wstring> m_sources;
        std::atomic<uint64_t> m_revision{ 0 };
        std::atomic<bool> m_scanning{ false };
    };
}
