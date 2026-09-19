#pragma once
//
// CoreUpdater.h
//
// Downloads/updates libretro cores from the libretro buildbot into the app's
// writable LocalFolder\Cores, since a packaged UWP app can't write into its
// own InstalledLocation at runtime. Your core loader should look in BOTH
// InstalledLocation\Assets\Cores (cores you shipped) and LocalFolder\Cores
// (anything this class has since updated), preferring the latter if both
// have the same core.
//
// Dependency: this uses miniz for zip extraction (Windows has no built-in
// WinRT API for reading arbitrary zip archives). Add miniz to the project -
// either `vcpkg install miniz` or drop the single-file amalgamated
// miniz.h/miniz.c from https://github.com/richgel999/miniz directly into
// the project; it's plain C with no OS-specific calls, so it's fine inside
// the UWP app container.
//
// Also requires the `internetClient` capability in Package.appxmanifest.
//
// NOTE: written without a Windows/WinRT toolchain to compile against, so
// treat this as a solid starting point rather than drop-in-and-done code -
// double check the Windows.Web.Http.Headers lookups and the miniz calls
// against your actual SDK/miniz version when you wire it in.

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.Storage.h>
#include <string>
#include <vector>
#include <functional>

namespace RetroForgeX
{
    // Describes one core RetroForgeX knows how to fetch.
    struct CoreInfo
    {
        std::wstring name;          // e.g. L"snes9x"       - used to look cores up by name
        std::wstring dllFileName;   // e.g. L"snes9x_libretro.dll"
        std::wstring zipFileName;   // e.g. L"snes9x_libretro.dll.zip"  (as named on buildbot)
    };

    struct UpdateResult
    {
        std::wstring coreName;
        bool updated = false;    // true only if a new file was actually written
        bool succeeded = false;  // true if the check/update completed without error
        std::wstring message;
    };

    class CoreUpdater
    {
    public:
        CoreUpdater();

        // Add a core this updater is responsible for. Call once per core at startup.
        void RegisterCore(const CoreInfo& core);

        // Checks every registered core against the buildbot's Last-Modified header
        // and updates any that are missing or stale. `callback` fires once per core,
        // on whichever thread the coroutine happens to resume on - marshal back to
        // the UI thread yourself if you touch UI from it.
        winrt::Windows::Foundation::IAsyncAction CheckAndUpdateAllAsync(
            std::function<void(const UpdateResult&)> callback);

        // Re-downloads one core unconditionally (skips the staleness check),
        // looked up by the `name` passed to RegisterCore.
        winrt::Windows::Foundation::IAsyncAction ForceUpdateCoreAsync(
            std::wstring coreName, std::function<void(const UpdateResult&)> callback);

        // Folder where updated/downloaded cores are written and should be loaded
        // from at runtime. Creates it on first call.
        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::StorageFolder>
            GetCoresFolderAsync();

    private:
        // Does the actual check-then-maybe-download work for one core.
        // If force is false: compares buildbot's Last-Modified against the sidecar
        // .meta file written by the previous update, and returns early if unchanged.
        // If force is true: skips the check and downloads unconditionally.
        winrt::Windows::Foundation::IAsyncOperation<bool> TryUpdateCoreAsync(
            CoreInfo core, bool force, UpdateResult& outResult);

        std::wstring GetBaseUrl() const;
        std::wstring GetMetadataFileName(const CoreInfo& core) const;

        std::vector<CoreInfo> m_cores;
        winrt::Windows::Web::Http::HttpClient m_httpClient;

        // Change these if you want to pin to a stable release instead of nightly,
        // e.g. m_channel = L"stable", m_platform/m_arch stay the same.
        std::wstring m_channel = L"nightly";
        std::wstring m_platform = L"windows";
        std::wstring m_arch = L"x86_64";
    };
}
