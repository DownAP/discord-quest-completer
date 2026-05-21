#include "completer.h"

#include <windows.h>
#include <filesystem>
#include <stdexcept>

namespace fs = std::filesystem;

namespace
{
    std::vector<std::shared_ptr<LaunchTask>> g_tasks;

    std::wstring Utf8ToWide(const std::string& s)
    {
        if (s.empty())
            return L"";
        const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
        std::wstring w(n, L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
        return w;
    }

    std::wstring NewestMatch(const std::wstring& dir, const std::wstring& pattern)
    {
        WIN32_FIND_DATAW fd;
        HANDLE h = ::FindFirstFileW((dir + L"\\" + pattern).c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE)
            return L"";

        std::wstring best;
        ULONGLONG    bestTime = 0;
        do
        {
            const std::wstring name = fd.cFileName;
            if (name == L"." || name == L"..")
                continue;
            const ULONGLONG t = ((ULONGLONG)fd.ftLastWriteTime.dwHighDateTime << 32)
                              | fd.ftLastWriteTime.dwLowDateTime;
            if (best.empty() || t > bestTime)
            {
                best     = name;
                bestTime = t;
            }
        } while (::FindNextFileW(h, &fd));
        ::FindClose(h);
        return best;
    }

    std::wstring ResolveWildcards(const std::wstring& full)
    {
        if (full.find(L'*') == std::wstring::npos)
            return full;

        std::vector<std::wstring> parts;
        size_t start = 0;
        for (size_t i = 0; i <= full.size(); ++i)
            if (i == full.size() || full[i] == L'\\')
            {
                parts.push_back(full.substr(start, i - start));
                start = i + 1;
            }

        std::wstring cur = parts.empty() ? L"" : parts[0];
        for (size_t i = 1; i < parts.size(); ++i)
        {
            if (parts[i].empty())
                continue;
            if (parts[i].find(L'*') != std::wstring::npos)
            {
                const std::wstring match = NewestMatch(cur, parts[i]);
                cur += L"\\" + (match.empty() ? parts[i] : match);
            }
            else
            {
                cur += L"\\" + parts[i];
            }
        }
        return cur;
    }

    std::wstring ResolveGamePath(const std::string& path)
    {
        std::wstring p = Utf8ToWide(path);
        for (auto& c : p)
            if (c == L'/')
                c = L'\\';

        wchar_t expanded[1024];
        const DWORD n = ::ExpandEnvironmentStringsW(p.c_str(), expanded, 1024);
        if (n > 0 && n <= 1024)
            p = expanded;

        const bool drive = p.size() >= 2 && p[1] == L':';
        const bool unc   = p.size() >= 2 && p[0] == L'\\' && p[1] == L'\\';

        std::wstring full;
        if (drive || unc)
        {
            full = p;
        }
        else
        {
            while (!p.empty() && p.front() == L'\\')
                p.erase(p.begin());
            full = SteamCommonPath() + L"\\" + p;
        }
        return ResolveWildcards(full);
    }

    std::wstring SelfExePath()
    {
        wchar_t buf[MAX_PATH];
        const DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
        return std::wstring(buf, n);
    }

    std::wstring CreateDirsTracked(const fs::path& dir)
    {
        std::error_code ec;

        std::vector<fs::path> chain;
        for (fs::path c = dir; !c.empty() && c != c.root_path(); c = c.parent_path())
            chain.push_back(c);

        std::wstring topMissing;
        for (auto it = chain.rbegin(); it != chain.rend(); ++it)
        {
            if (!fs::exists(*it, ec))
            {
                topMissing = it->wstring();
                break;
            }
        }

        fs::create_directories(dir, ec);
        if (!fs::exists(dir, ec))
            throw std::runtime_error("Could not create the game folder.");
        return topMissing;
    }

    bool RunProcessAndWait(const std::wstring& exePath, long long argMs,
                           const std::atomic<bool>& stop)
    {
        std::wstring cmd = L"\"" + exePath + L"\" " + std::to_wstring(argMs);
        std::vector<wchar_t> buf(cmd.begin(), cmd.end());
        buf.push_back(L'\0');

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        if (!::CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE,
                              0, nullptr, nullptr, &si, &pi))
            return false;

        while (::WaitForSingleObject(pi.hProcess, 150) == WAIT_TIMEOUT)
        {
            if (stop.load())
            {
                ::TerminateProcess(pi.hProcess, 0);
                ::WaitForSingleObject(pi.hProcess, 3000);
                break;
            }
        }
        ::CloseHandle(pi.hProcess);
        ::CloseHandle(pi.hThread);
        return true;
    }

    void Worker(std::shared_ptr<LaunchTask> task, int seconds)
    {
        const fs::path full(task->fullPath);
        const fs::path dir    = full.parent_path();
        const fs::path backup = dir / L"old_game_file.exe";

        std::wstring topCreated;
        bool         renamedOriginal = false;

        auto restore = [&]()
        {
            std::error_code e;
            if (!topCreated.empty())
            {
                fs::remove_all(fs::path(topCreated), e);
            }
            else
            {
                fs::remove(full, e);
                if (renamedOriginal && fs::exists(backup, e))
                    fs::rename(backup, full, e);
            }
        };

        try
        {
            std::error_code ec;
            task->SetMessage("Preparing game folder...");
            topCreated = CreateDirsTracked(dir);

            if (topCreated.empty() && fs::exists(full, ec))
            {
                fs::remove(backup, ec);
                fs::rename(full, backup);
                renamedOriginal = true;
            }

            fs::copy_file(SelfExePath(), full, fs::copy_options::overwrite_existing);

            task->SetMessage("Keep this app open.");
            if (!RunProcessAndWait(full.wstring(), (long long)seconds * 1000,
                                   task->stopRequested))
                throw std::runtime_error("the simulated game process would not start.");

            restore();
            if (task->stopRequested.load())
            {
                task->SetMessage("Stopped early.");
                task->state.store(TaskState::Stopped);
            }
            else
            {
                task->SetMessage("Done - quest time completed.");
                task->state.store(TaskState::Finished);
            }
        }
        catch (const std::exception& ex)
        {
            restore();
            task->SetMessage(std::string("Failed: ") + ex.what());
            task->state.store(TaskState::Failed);
        }
        catch (...)
        {
            restore();
            task->SetMessage("Failed: unknown error.");
            task->state.store(TaskState::Failed);
        }
    }
}

void LaunchTask::SetMessage(std::string m)
{
    std::lock_guard<std::mutex> lock(msgMutex);
    message = std::move(m);
}

std::string LaunchTask::Message() const
{
    std::lock_guard<std::mutex> lock(msgMutex);
    return message;
}

void LaunchTask::RequestStop()
{
    float p = 1.0f;
    if (durationSeconds > 0)
    {
        const float elapsed =
            std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
        p = elapsed / (float)durationSeconds;
        if (p < 0.0f) p = 0.0f;
        if (p > 1.0f) p = 1.0f;
    }
    stoppedAt.store(p);
    stopRequested.store(true);
    SetMessage("Stopping...");
}

float LaunchTask::Progress() const
{
    const float frozen = stoppedAt.load();
    if (frozen >= 0.0f)
        return frozen;
    if (state.load() != TaskState::Running)
        return 1.0f;
    if (durationSeconds <= 0)
        return 1.0f;
    const float elapsed =
        std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
    const float p = elapsed / (float)durationSeconds;
    return p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
}

int LaunchTask::SecondsLeft() const
{
    if (state.load() != TaskState::Running || stopRequested.load())
        return 0;
    const float elapsed =
        std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();
    const int left = durationSeconds - (int)elapsed;
    return left < 0 ? 0 : left;
}

std::wstring SteamCommonPath()
{
    std::wstring base;

    HKEY key;
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam",
                        0, KEY_READ, &key) == ERROR_SUCCESS)
    {
        wchar_t buf[MAX_PATH];
        DWORD   sz   = sizeof(buf);
        DWORD   type = 0;
        if (::RegQueryValueExW(key, L"SteamPath", nullptr, &type,
                               (LPBYTE)buf, &sz) == ERROR_SUCCESS && type == REG_SZ)
            base.assign(buf, sz / sizeof(wchar_t));
        ::RegCloseKey(key);
    }

    while (!base.empty() && (base.back() == L'\0' || base.back() == L'\\' || base.back() == L'/'))
        base.pop_back();
    if (base.empty())
        base = L"C:\\Program Files (x86)\\Steam";
    for (auto& c : base)
        if (c == L'/')
            c = L'\\';

    return base + L"\\steamapps\\common";
}

std::shared_ptr<LaunchTask> StartLaunch(const std::string& name,
                                        const std::string& relPath,
                                        int seconds)
{
    auto task = std::make_shared<LaunchTask>();
    task->name            = name;
    task->fullPath        = ResolveGamePath(relPath);
    task->durationSeconds = seconds;
    task->startTime       = std::chrono::steady_clock::now();

    g_tasks.push_back(task);
    task->worker = std::thread(Worker, task, seconds);
    return task;
}

void PumpTasks()
{
    for (auto& t : g_tasks)
        if (t->state.load() != TaskState::Running && t->worker.joinable())
            t->worker.join();
}

const std::vector<std::shared_ptr<LaunchTask>>& ActiveTasks()
{
    return g_tasks;
}

void WaitAllTasks()
{
    for (auto& t : g_tasks)
        if (t->worker.joinable())
            t->worker.join();
}

bool IsPathBusy(const std::string& relPath)
{
    const std::wstring full = ResolveGamePath(relPath);
    for (const auto& t : g_tasks)
        if (t->state.load() == TaskState::Running &&
            _wcsicmp(t->fullPath.c_str(), full.c_str()) == 0)
            return true;
    return false;
}
