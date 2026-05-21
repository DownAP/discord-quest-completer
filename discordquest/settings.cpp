#include "settings.h"

#include <windows.h>
#include <wininet.h>

#include "json.hpp"

#pragma comment(lib, "wininet.lib")

const char* const kSettingsUrl =
    "https://raw.githubusercontent.com/DownAP/discord-quest-completer/master/settings.json";

namespace
{
    bool HttpGet(const char* url, std::string& out, std::string& err)
    {
        HINTERNET net = ::InternetOpenA("DiscordQuestCompleter/1.0",
                                        INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
        if (!net)
        {
            err = "Could not initialise the network stack.";
            return false;
        }

        const DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE |
                            INTERNET_FLAG_SECURE | INTERNET_FLAG_PRAGMA_NOCACHE;
        HINTERNET req = ::InternetOpenUrlA(net, url, nullptr, 0, flags, 0);
        if (!req)
        {
            err = "Could not reach the settings URL (no internet?).";
            ::InternetCloseHandle(net);
            return false;
        }

        DWORD status = 0, len = sizeof(status);
        ::HttpQueryInfoA(req, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                         &status, &len, nullptr);
        if (status != 200)
        {
            err = "Settings URL returned HTTP " + std::to_string(status) + ".";
            ::InternetCloseHandle(req);
            ::InternetCloseHandle(net);
            return false;
        }

        out.clear();
        char buf[8192];
        DWORD read = 0;
        while (::InternetReadFile(req, buf, sizeof(buf), &read) && read > 0)
            out.append(buf, read);

        ::InternetCloseHandle(req);
        ::InternetCloseHandle(net);
        return true;
    }
}

Settings FetchSettings()
{
    Settings s;

    std::string body, err;
    if (!HttpGet(kSettingsUrl, body, err))
    {
        s.error = err;
        return s;
    }

    try
    {
        const nlohmann::json j = nlohmann::json::parse(body);
        for (const auto& g : j.at("games"))
        {
            GameEntry e;
            e.name    = g.at("name").get<std::string>();
            e.path    = g.at("path").get<std::string>();
            e.seconds = g.value("seconds", 900);
            if (!e.name.empty() && !e.path.empty() && e.seconds > 0)
                s.games.push_back(std::move(e));
        }
        s.ok = true;
    }
    catch (const std::exception& ex)
    {
        s.error = std::string("settings.json is malformed: ") + ex.what();
    }

    return s;
}
