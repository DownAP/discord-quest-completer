#pragma once
#include <string>
#include <vector>

struct GameEntry
{
    std::string name;
    std::string path;          // relative to steamapps\common
    int         seconds = 900;
};

struct Settings
{
    std::vector<GameEntry> games;
    bool        ok = false;
    std::string error;
};

extern const char* const kSettingsUrl;

Settings FetchSettings();
