#include "pch.h"
#include "RomLibrary.h"
#include "Core/LibretroCore.h"

using namespace winrt;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Search;
using namespace winrt::Windows::Storage::AccessCache;

namespace rfx
{
    namespace
    {
        std::string ToLower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        std::vector<std::string> SplitExtensions(std::string const& list)
        {
            std::vector<std::string> out;
            std::string cur;
            for (char c : list)
            {
                if (c == '|')
                {
                    if (!cur.empty()) out.push_back(ToLower(cur));
                    cur.clear();
                }
                else if (c != '.' && c != ' ')
                {
                    cur.push_back(c);
                }
            }
            if (!cur.empty()) out.push_back(ToLower(cur));
            return out;
        }

        StorageFolder TryGetFolder(std::wstring const& path)
        {
            try
            {
                return StorageFolder::GetFolderFromPathAsync(path).get();
            }
            catch (...)
            {
                return nullptr;
            }
        }

        StorageFolder TryGetChildFolder(StorageFolder const& parent, wchar_t const* name)
        {
            try
            {
                auto item = parent.TryGetItemAsync(name).get();
                return item ? item.try_as<StorageFolder>() : nullptr;
            }
            catch (...)
            {
                return nullptr;
            }
        }

        // Parses a libretro .info file (key = "value" lines). Returns false if missing.
        bool ReadCoreInfo(std::wstring const& path, std::map<std::string, std::string>& out)
        {
            std::ifstream in(path);
            if (!in)
            {
                return false;
            }
            std::string line;
            while (std::getline(in, line))
            {
                auto eq = line.find('=');
                if (eq == std::string::npos || line[0] == '#')
                {
                    continue;
                }
                std::string key = line.substr(0, eq);
                std::string value = line.substr(eq + 1);
                auto trim = [](std::string& t)
                {
                    while (!t.empty() && (t.back() == ' ' || t.back() == '\r' || t.back() == '"')) t.pop_back();
                    size_t i = 0;
                    while (i < t.size() && (t[i] == ' ' || t[i] == '"')) ++i;
                    t.erase(0, i);
                };
                trim(key);
                trim(value);
                out[key] = value;
            }
            return true;
        }
    }

    void RomLibrary::ScanCores()
    {
        std::vector<CoreEntry> cores;
        try
        {
            auto installed = winrt::Windows::ApplicationModel::Package::Current().InstalledLocation();
            auto coresFolder = TryGetChildFolder(installed, L"Cores");
            if (coresFolder)
            {
                for (auto const& file : coresFolder.GetFilesAsync().get())
                {
                    std::wstring name{ file.Name() };
                    if (name.size() < 4 || _wcsicmp(name.c_str() + name.size() - 4, L".dll") != 0)
                    {
                        continue;
                    }
                    CoreEntry entry;
                    entry.fileName = name;
                    entry.packagePath = L"Cores\\" + name;

                    // Prefer the libretro .info file next to the dll: it avoids loading
                    // every core (some are 50+ MB) just to list it.
                    std::map<std::string, std::string> info;
                    std::wstring infoPath = std::wstring{ coresFolder.Path() } + L"\\" +
                        name.substr(0, name.size() - 4) + L".info";
                    if (ReadCoreInfo(infoPath, info) && !info["supported_extensions"].empty())
                    {
                        entry.name = !info["display_name"].empty() ? info["display_name"] : winrt::to_string(name);
                        entry.version = info["display_version"];
                        entry.extensions = SplitExtensions(info["supported_extensions"]);
                        entry.needFullPath = info["needs_fullpath"] == "true";
                    }
                    else
                    {
                        LibretroCore::ProbeResult probe;
                        if (!LibretroCore::Probe(entry.packagePath, probe))
                        {
                            OutputDebugStringW((L"[RomLibrary] Could not load core " + name + L"\n").c_str());
                            continue;
                        }
                        entry.name = probe.libraryName.empty() ? winrt::to_string(name) : probe.libraryName;
                        entry.version = probe.libraryVersion;
                        entry.extensions = SplitExtensions(probe.validExtensions);
                        entry.needFullPath = probe.needFullPath;
                    }
                    cores.push_back(std::move(entry));
                }
            }
        }
        catch (...)
        {
        }

        std::sort(cores.begin(), cores.end(), [](CoreEntry const& a, CoreEntry const& b) { return a.name < b.name; });
        {
            std::lock_guard lock(m_mutex);
            m_cores = std::move(cores);
        }
        ++m_revision;
    }

    void RomLibrary::AddFolder(StorageFolder const& folder)
    {
        try
        {
            StorageApplicationPermissions::FutureAccessList().Add(folder, folder.Path());
        }
        catch (...)
        {
        }
    }

    void RomLibrary::ScanFolder(StorageFolder const& folder, std::vector<GameEntry>& out,
        std::map<std::wstring, bool>& seen, std::vector<CoreEntry> const& cores, std::vector<hstring> const& filter)
    {
        if (!folder)
        {
            return;
        }
        std::wstring root{ folder.Path() };
        if (seen.count(root))
        {
            return;
        }
        seen[root] = true;

        try
        {
            QueryOptions options;
            options.FolderDepth(FolderDepth::Deep);
            options.IndexerOption(IndexerOption::DoNotUseIndexer);
            for (auto const& ext : filter)
            {
                options.FileTypeFilter().Append(ext);
            }
            auto query = folder.CreateFileQueryWithOptions(options);
            auto files = query.GetFilesAsync().get();

            {
                std::lock_guard lock(m_mutex);
                m_sources.push_back(root);
            }

            for (auto const& file : files)
            {
                std::wstring path{ file.Path() };
                if (seen.count(path))
                {
                    continue;
                }
                seen[path] = true;

                GameEntry g;
                g.path = path;
                g.file = file;
                g.displayName = winrt::to_string(file.DisplayName());
                std::string ext = winrt::to_string(file.FileType());
                g.extension = ToLower(ext.empty() ? ext : ext.substr(1));
                for (size_t c = 0; c < cores.size(); ++c)
                {
                    const auto& exts = cores[c].extensions;
                    if (std::find(exts.begin(), exts.end(), g.extension) != exts.end())
                    {
                        g.coreIndices.push_back(static_cast<int>(c));
                    }
                }
                if (!g.coreIndices.empty())
                {
                    out.push_back(std::move(g));
                }
                if (out.size() >= 20000)
                {
                    break;
                }
            }
        }
        catch (...)
        {
            OutputDebugStringW((L"[RomLibrary] Could not scan " + root + L"\n").c_str());
        }
    }

    void RomLibrary::ScanGames()
    {
        bool expected = false;
        if (!m_scanning.compare_exchange_strong(expected, true))
        {
            return;
        }

        std::vector<CoreEntry> cores = Cores();
        std::vector<hstring> filter;
        {
            std::map<std::string, bool> unique;
            for (auto const& c : cores)
                for (auto const& e : c.extensions)
                    unique[e] = true;
            for (auto const& [e, _] : unique)
                filter.push_back(winrt::to_hstring("." + e));
        }

        {
            std::lock_guard lock(m_mutex);
            m_sources.clear();
        }

        std::vector<GameEntry> games;
        std::map<std::wstring, bool> seen;

        if (!filter.empty())
        {
            // 1. LocalState\roms
            try
            {
                auto local = ApplicationData::Current().LocalFolder();
                auto roms = local.CreateFolderAsync(L"roms", CreationCollisionOption::OpenIfExists).get();
                ScanFolder(roms, games, seen, cores, filter);
            }
            catch (...)
            {
            }

            // 2. Picked folders
            try
            {
                for (auto const& entry : StorageApplicationPermissions::FutureAccessList().Entries())
                {
                    try
                    {
                        auto folder = StorageApplicationPermissions::FutureAccessList().GetFolderAsync(entry.Token).get();
                        ScanFolder(folder, games, seen, cores, filter);
                    }
                    catch (...)
                    {
                    }
                }
            }
            catch (...)
            {
            }

            // 3. <drive>:\roms via broadFileSystemAccess
            for (wchar_t drive = L'D'; drive <= L'K'; ++drive)
            {
                std::wstring path = std::wstring(1, drive) + L":\\roms";
                if (auto folder = TryGetFolder(path))
                {
                    ScanFolder(folder, games, seen, cores, filter);
                }
            }

            // 4. Removable devices
            try
            {
                auto removable = KnownFolders::RemovableDevices();
                for (auto const& drive : removable.GetFoldersAsync().get())
                {
                    if (auto roms = TryGetChildFolder(drive, L"roms"))
                    {
                        ScanFolder(roms, games, seen, cores, filter);
                    }
                }
            }
            catch (...)
            {
            }
        }

        std::sort(games.begin(), games.end(), [](GameEntry const& a, GameEntry const& b)
        {
            return _stricmp(a.displayName.c_str(), b.displayName.c_str()) < 0;
        });

        {
            std::lock_guard lock(m_mutex);
            m_games = std::move(games);
        }
        ++m_revision;
        m_scanning = false;
    }

    std::vector<CoreEntry> RomLibrary::Cores() const
    {
        std::lock_guard lock(m_mutex);
        return m_cores;
    }

    std::vector<GameEntry> RomLibrary::Games() const
    {
        std::lock_guard lock(m_mutex);
        return m_games;
    }

    std::vector<std::wstring> RomLibrary::SourceFolders() const
    {
        std::lock_guard lock(m_mutex);
        return m_sources;
    }

    bool RomLibrary::ReadAllBytes(StorageFile const& file, std::vector<uint8_t>& out)
    {
        try
        {
            auto buffer = FileIO::ReadBufferAsync(file).get();
            out.resize(buffer.Length());
            if (!out.empty())
            {
                std::memcpy(out.data(), buffer.data(), out.size());
            }
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    std::wstring RomLibrary::EnsureDirectlyReadable(StorageFile const& file)
    {
        std::wstring path{ file.Path() };
        HANDLE h = CreateFile2(path.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
        if (h != INVALID_HANDLE_VALUE)
        {
            CloseHandle(h);
            return path;
        }
        try
        {
            auto local = ApplicationData::Current().LocalFolder();
            auto cache = local.CreateFolderAsync(L"cache", CreationCollisionOption::OpenIfExists).get();
            auto copy = file.CopyAsync(cache, file.Name(), NameCollisionOption::ReplaceExisting).get();
            return std::wstring{ copy.Path() };
        }
        catch (...)
        {
            return path;
        }
    }
}
