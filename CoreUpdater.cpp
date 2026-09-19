#include "pch.h"
#include "CoreUpdater.h"

#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <miniz.h>
#include <cstring>

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Web::Http;
using namespace winrt::Windows::Web::Http::Headers;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;

namespace RetroForgeX
{
    CoreUpdater::CoreUpdater()
    {
        m_httpClient.DefaultRequestHeaders().UserAgent().TryParseAdd(L"RetroForgeX-CoreUpdater/1.0");
    }

    void CoreUpdater::RegisterCore(const CoreInfo& core)
    {
        m_cores.push_back(core);
    }

    std::wstring CoreUpdater::GetBaseUrl() const
    {
        return L"https://buildbot.libretro.com/" + m_channel + L"/" + m_platform + L"/" + m_arch + L"/latest/";
    }

    std::wstring CoreUpdater::GetMetadataFileName(const CoreInfo& core) const
    {
        return core.dllFileName + L".meta";
    }

    IAsyncOperation<StorageFolder> CoreUpdater::GetCoresFolderAsync()
    {
        auto local = ApplicationData::Current().LocalFolder();
        co_return co_await local.CreateFolderAsync(L"Cores", CreationCollisionOption::OpenIfExists);
    }

    IAsyncOperation<bool> CoreUpdater::TryUpdateCoreAsync(CoreInfo core, bool force, UpdateResult& outResult)
    {
        outResult.coreName = core.name;
        Uri uri{ GetBaseUrl() + core.zipFileName };
        auto coresFolder = co_await GetCoresFolderAsync();

        winrt::hstring remoteLastModified;

        if (!force)
        {
            // Cheap staleness check first: HEAD request, compare Last-Modified
            // against what we stashed the last time we actually downloaded this core.
            HttpRequestMessage headRequest{ HttpMethod::Head(), uri };
            try
            {
                auto headResponse = co_await m_httpClient.SendRequestAsync(headRequest);
                headResponse.EnsureSuccessStatusCode();

                if (headResponse.Content().Headers().HasKey(L"Last-Modified"))
                {
                    remoteLastModified = headResponse.Content().Headers().Lookup(L"Last-Modified");
                }
            }
            catch (const hresult_error&)
            {
                outResult.succeeded = false;
                outResult.message = L"Couldn't reach buildbot.libretro.com";
                co_return false;
            }

            IStorageItem existingDll{ nullptr };
            try { existingDll = co_await coresFolder.TryGetItemAsync(core.dllFileName); }
            catch (...) {}

            if (existingDll && !remoteLastModified.empty())
            {
                try
                {
                    auto metaFile = co_await coresFolder.GetFileAsync(GetMetadataFileName(core));
                    auto stored = co_await FileIO::ReadTextAsync(metaFile);
                    if (stored == remoteLastModified)
                    {
                        outResult.succeeded = true;
                        outResult.updated = false;
                        outResult.message = L"Already up to date";
                        co_return true;
                    }
                }
                catch (...)
                {
                    // dll exists but no .meta sidecar yet (e.g. it was copied in by
                    // hand originally) - fall through and adopt buildbot tracking now.
                }
            }
        }

        // --- Missing, stale, or forced: download and replace ---
        try
        {
            auto response = co_await m_httpClient.GetAsync(uri);
            response.EnsureSuccessStatusCode();

            if (response.Content().Headers().HasKey(L"Last-Modified"))
            {
                remoteLastModified = response.Content().Headers().Lookup(L"Last-Modified");
            }

            IBuffer zipBuffer = co_await response.Content().ReadAsBufferAsync();
            std::vector<uint8_t> zipBytes(zipBuffer.Length());
            std::memcpy(zipBytes.data(), zipBuffer.data(), zipBuffer.Length());

            mz_zip_archive zipArchive{};
            if (!mz_zip_reader_init_mem(&zipArchive, zipBytes.data(), zipBytes.size(), 0))
            {
                outResult.succeeded = false;
                outResult.message = L"Downloaded file wasn't a valid zip";
                co_return false;
            }

            // Cores are zipped as a single file with the same name as the dll.
            std::string dllNameNarrow(core.dllFileName.begin(), core.dllFileName.end());
            int fileIndex = mz_zip_reader_locate_file(&zipArchive, dllNameNarrow.c_str(), nullptr, 0);
            if (fileIndex < 0)
            {
                mz_zip_reader_end(&zipArchive);
                outResult.succeeded = false;
                outResult.message = L"Archive didn't contain " + core.dllFileName + L" (buildbot layout may have changed)";
                co_return false;
            }

            size_t extractedSize = 0;
            void* extractedData = mz_zip_reader_extract_to_heap(&zipArchive, fileIndex, &extractedSize, 0);
            mz_zip_reader_end(&zipArchive);

            if (!extractedData)
            {
                outResult.succeeded = false;
                outResult.message = L"Failed to extract " + core.dllFileName + L" from the archive";
                co_return false;
            }

            DataWriter writer;
            writer.WriteBytes(array_view<const uint8_t>(
                reinterpret_cast<const uint8_t*>(extractedData),
                reinterpret_cast<const uint8_t*>(extractedData) + extractedSize));
            mz_free(extractedData);

            auto dllFile = co_await coresFolder.CreateFileAsync(core.dllFileName, CreationCollisionOption::ReplaceExisting);
            co_await FileIO::WriteBufferAsync(dllFile, writer.DetachBuffer());

            if (!remoteLastModified.empty())
            {
                auto metaFile = co_await coresFolder.CreateFileAsync(GetMetadataFileName(core), CreationCollisionOption::ReplaceExisting);
                co_await FileIO::WriteTextAsync(metaFile, remoteLastModified);
            }

            outResult.succeeded = true;
            outResult.updated = true;
            outResult.message = L"Updated";
            co_return true;
        }
        catch (const hresult_error& ex)
        {
            outResult.succeeded = false;
            outResult.message = ex.message();
            co_return false;
        }
    }

    IAsyncAction CoreUpdater::CheckAndUpdateAllAsync(std::function<void(const UpdateResult&)> callback)
    {
        for (auto core : m_cores)
        {
            UpdateResult result;
            co_await TryUpdateCoreAsync(core, /*force*/ false, result);
            if (callback)
            {
                callback(result);
            }
        }
    }

    IAsyncAction CoreUpdater::ForceUpdateCoreAsync(std::wstring coreName, std::function<void(const UpdateResult&)> callback)
    {
        for (auto& core : m_cores)
        {
            if (core.name == coreName)
            {
                UpdateResult result;
                co_await TryUpdateCoreAsync(core, /*force*/ true, result);
                if (callback)
                {
                    callback(result);
                }
                co_return;
            }
        }

        if (callback)
        {
            UpdateResult result;
            result.coreName = coreName;
            result.succeeded = false;
            result.message = L"No core registered with that name";
            callback(result);
        }
    }
}

// --- Example usage, e.g. in App.cpp during startup ---
//
// RetroForgeX::CoreUpdater updater;
// updater.RegisterCore({ L"snes9x", L"snes9x_libretro.dll", L"snes9x_libretro.dll.zip" });
// updater.RegisterCore({ L"mgba",   L"mgba_libretro.dll",   L"mgba_libretro.dll.zip" });
//
// updater.CheckAndUpdateAllAsync([](const RetroForgeX::UpdateResult& r)
// {
//     // e.g. log r.coreName / r.updated / r.message, or push a toast to the UI
// });