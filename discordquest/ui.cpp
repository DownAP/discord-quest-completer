#include "ui.h"
#include "render.h"
#include "completer.h"
#include "settings.h"

#include "imgui.h"

#include <windows.h>
#include <atomic>
#include <cfloat>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

namespace
{
    const ImVec4 COL_TEXT      = ImVec4(0.88f, 0.89f, 0.91f, 1.00f);
    const ImVec4 COL_MUTED     = ImVec4(0.55f, 0.57f, 0.62f, 1.00f);
    const ImVec4 COL_ACCENT    = ImVec4(0.345f, 0.396f, 0.949f, 1.00f);
    const ImVec4 COL_ACCENT_H  = ImVec4(0.42f, 0.46f, 0.97f, 1.00f);
    const ImVec4 COL_ACCENT_A  = ImVec4(0.30f, 0.34f, 0.86f, 1.00f);
    const ImVec4 COL_GREEN     = ImVec4(0.30f, 0.78f, 0.47f, 1.00f);
    const ImVec4 COL_RED       = ImVec4(0.93f, 0.38f, 0.42f, 1.00f);
    const ImVec4 COL_CARD      = ImVec4(0.160f, 0.170f, 0.195f, 1.00f);
    const ImVec4 COL_CARD_HOV  = ImVec4(0.205f, 0.215f, 0.250f, 1.00f);
    const ImVec4 COL_CARD_SEL  = ImVec4(0.220f, 0.235f, 0.330f, 1.00f);

    ImFont* g_fontUI       = nullptr;
    ImFont* g_fontSemibold = nullptr;

    ImFont* TryFont(ImFontAtlas* atlas, const char* path, float size)
    {
        if (::GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
            return nullptr;
        return atlas->AddFontFromFileTTF(path, size);
    }

    std::string WideToUtf8(const std::wstring& w)
    {
        if (w.empty())
            return "";
        const int n = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
                                            nullptr, 0, nullptr, nullptr);
        std::string s(n, '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(),
                              s.data(), n, nullptr, nullptr);
        return s;
    }

    std::string FormatDuration(int totalSeconds)
    {
        if (totalSeconds <= 0)
            return "0s";
        const int h = totalSeconds / 3600;
        const int m = (totalSeconds % 3600) / 60;
        const int s = totalSeconds % 60;
        std::string r;
        if (h) r += std::to_string(h) + "h ";
        if (m) r += std::to_string(m) + "m ";
        if (s || r.empty()) r += std::to_string(s) + "s";
        while (!r.empty() && r.back() == ' ')
            r.pop_back();
        return r;
    }
}

void SetupImGuiStyle()
{
    ImGuiIO&    io = ImGui::GetIO();
    ImGuiStyle& st = ImGui::GetStyle();

    g_fontUI = TryFont(io.Fonts, "C:\\Windows\\Fonts\\segoeui.ttf", 16.0f);
    if (!g_fontUI)
        g_fontUI = io.Fonts->AddFontDefault();

    g_fontSemibold = TryFont(io.Fonts, "C:\\Windows\\Fonts\\seguisb.ttf", 16.0f);
    if (!g_fontSemibold)
        g_fontSemibold = TryFont(io.Fonts, "C:\\Windows\\Fonts\\segoeuib.ttf", 16.0f);
    if (!g_fontSemibold)
        g_fontSemibold = g_fontUI;

    st.FontSizeBase = 16.0f;

    st.WindowRounding    = 0.0f;
    st.ChildRounding     = 10.0f;
    st.FrameRounding     = 7.0f;
    st.GrabRounding      = 7.0f;
    st.PopupRounding     = 8.0f;
    st.ScrollbarRounding = 8.0f;
    st.TabRounding       = 7.0f;
    st.WindowPadding     = ImVec2(18, 16);
    st.FramePadding      = ImVec2(11, 8);
    st.ItemSpacing       = ImVec2(9, 9);
    st.ItemInnerSpacing  = ImVec2(7, 5);
    st.ScrollbarSize     = 11.0f;
    st.WindowBorderSize  = 0.0f;
    st.ChildBorderSize   = 1.0f;
    st.FrameBorderSize   = 0.0f;
    st.GrabMinSize       = 14.0f;

    ImVec4* c = st.Colors;
    c[ImGuiCol_Text]                  = COL_TEXT;
    c[ImGuiCol_TextDisabled]          = COL_MUTED;
    c[ImGuiCol_WindowBg]              = ImVec4(0.105f, 0.110f, 0.122f, 1.00f);
    c[ImGuiCol_ChildBg]               = ImVec4(0.130f, 0.137f, 0.150f, 1.00f);
    c[ImGuiCol_PopupBg]               = ImVec4(0.130f, 0.135f, 0.150f, 1.00f);
    c[ImGuiCol_Border]                = ImVec4(0.220f, 0.230f, 0.270f, 1.00f);
    c[ImGuiCol_FrameBg]               = ImVec4(0.165f, 0.175f, 0.195f, 1.00f);
    c[ImGuiCol_FrameBgHovered]        = ImVec4(0.200f, 0.210f, 0.240f, 1.00f);
    c[ImGuiCol_FrameBgActive]         = ImVec4(0.230f, 0.240f, 0.280f, 1.00f);
    c[ImGuiCol_Button]                = ImVec4(0.200f, 0.210f, 0.245f, 1.00f);
    c[ImGuiCol_ButtonHovered]         = ImVec4(0.245f, 0.255f, 0.300f, 1.00f);
    c[ImGuiCol_ButtonActive]          = ImVec4(0.285f, 0.300f, 0.360f, 1.00f);
    c[ImGuiCol_Header]                = COL_CARD_SEL;
    c[ImGuiCol_HeaderHovered]         = COL_CARD_HOV;
    c[ImGuiCol_HeaderActive]          = COL_CARD_SEL;
    c[ImGuiCol_SliderGrab]            = COL_ACCENT;
    c[ImGuiCol_SliderGrabActive]      = COL_ACCENT_H;
    c[ImGuiCol_CheckMark]             = COL_ACCENT;
    c[ImGuiCol_Separator]             = ImVec4(0.220f, 0.230f, 0.270f, 1.00f);
    c[ImGuiCol_SeparatorHovered]      = COL_ACCENT;
    c[ImGuiCol_ScrollbarBg]           = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);
    c[ImGuiCol_ScrollbarGrab]         = ImVec4(0.260f, 0.270f, 0.310f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.320f, 0.330f, 0.380f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]   = COL_ACCENT;
    c[ImGuiCol_PlotHistogram]         = COL_ACCENT;
    c[ImGuiCol_NavCursor]             = COL_ACCENT;
}

namespace
{
    void SmallCaps(const char* text)
    {
        ImGui::PushFont(g_fontSemibold, 13.0f);
        ImGui::TextColored(COL_MUTED, "%s", text);
        ImGui::PopFont();
    }

    bool GameCard(int id, const char* name, const char* sub,
                  const char* badge, bool selected)
    {
        ImGui::PushID(id);

        const float  height = 56.0f;
        const ImVec2 p0     = ImGui::GetCursorScreenPos();
        const ImVec2 size(ImGui::GetContentRegionAvail().x, height);

        const bool clicked = ImGui::InvisibleButton("##card", size);
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 p1(p0.x + size.x, p0.y + size.y);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImU32 bg = ImGui::GetColorU32(
            selected ? COL_CARD_SEL : (hovered ? COL_CARD_HOV : COL_CARD));
        dl->AddRectFilled(p0, p1, bg, 8.0f);
        if (selected)
        {
            dl->AddRect(p0, p1, ImGui::GetColorU32(COL_ACCENT), 8.0f, 0, 1.5f);
            dl->AddRectFilled(p0, ImVec2(p0.x + 4, p1.y),
                              ImGui::GetColorU32(COL_ACCENT), 8.0f,
                              ImDrawFlags_RoundCornersLeft);
        }

        const float scale = ImGui::GetFontSize() / ImGui::GetStyle().FontSizeBase;
        dl->AddText(g_fontSemibold, 16.0f * scale, ImVec2(p0.x + 16, p0.y + 8),
                    ImGui::GetColorU32(COL_TEXT), name);
        dl->AddText(g_fontUI, 13.0f * scale, ImVec2(p0.x + 16, p0.y + 31),
                    ImGui::GetColorU32(COL_MUTED), sub);

        if (badge && badge[0])
        {
            ImGui::PushFont(g_fontUI, 13.0f);
            const ImVec2 ts = ImGui::CalcTextSize(badge);
            ImGui::PopFont();
            dl->AddText(g_fontUI, 13.0f * scale,
                        ImVec2(p1.x - ts.x - 16, p0.y + (height - ts.y) * 0.5f),
                        ImGui::GetColorU32(COL_ACCENT_H), badge);
        }

        ImGui::PopID();
        return clicked;
    }

    void DrawTaskRow(LaunchTask& t)
    {
        ImGui::PushID(&t);

        const TaskState s = t.state.load();
        ImVec4      col;
        const char* word;
        switch (s)
        {
        case TaskState::Running:  col = COL_ACCENT; word = "RUNNING"; break;
        case TaskState::Finished: col = COL_GREEN;  word = "DONE";    break;
        case TaskState::Stopped:  col = COL_MUTED;  word = "STOPPED"; break;
        default:                  col = COL_RED;    word = "FAILED";  break;
        }

        ImGui::PushFont(g_fontSemibold, 15.0f);
        ImGui::TextUnformatted(t.name.c_str());
        ImGui::PopFont();

        ImGui::SameLine(0, 0);
        if (s == TaskState::Running)
        {
            const bool  stopping = t.stopRequested.load();
            const char* btn      = stopping ? "Stopping" : "Stop";
            ImGui::PushFont(g_fontSemibold, 12.0f);
            const float bw = ImGui::CalcTextSize(btn).x +
                             ImGui::GetStyle().FramePadding.x * 2.0f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                                 ImGui::GetContentRegionAvail().x - bw);
            ImGui::BeginDisabled(stopping);
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.42f, 0.20f, 0.22f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.56f, 0.24f, 0.26f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.62f, 0.26f, 0.28f, 1.0f));
            if (ImGui::SmallButton(btn))
                t.RequestStop();
            ImGui::PopStyleColor(3);
            ImGui::EndDisabled();
            ImGui::PopFont();
        }
        else
        {
            ImGui::PushFont(g_fontSemibold, 12.0f);
            const float ww = ImGui::CalcTextSize(word).x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                                 ImGui::GetContentRegionAvail().x - ww);
            ImGui::TextColored(col, "%s", word);
            ImGui::PopFont();
        }

        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, col);
        ImGui::ProgressBar(t.Progress(), ImVec2(-FLT_MIN, 9.0f), "");
        ImGui::PopStyleColor();

        ImGui::PushFont(g_fontUI, 13.0f);
        std::string line = t.Message();
        if (s == TaskState::Running && !t.stopRequested.load())
            line += "  -  " + FormatDuration(t.SecondsLeft()) + " left";
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(COL_MUTED, "%s", line.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopFont();

        ImGui::Dummy(ImVec2(0, 7));
        ImGui::PopID();
    }

    struct SettingsLoad
    {
        std::atomic<bool> done{ false };
        Settings          result;
    };

    void StartSettingsLoad(std::shared_ptr<SettingsLoad>& slot)
    {
        slot = std::make_shared<SettingsLoad>();
        auto holder = slot;
        std::thread([holder]()
        {
            holder->result = FetchSettings();
            holder->done.store(true);
        }).detach();
    }

    struct CompleterUI
    {
        std::shared_ptr<SettingsLoad> load;
        int  selectedGame = -1;
        bool useCustom    = false;
        int  seconds      = 900;
        char customName[128] = "";
        char customPath[512] = "";
    };

    void DrawQuestList(CompleterUI& ui)
    {
        SmallCaps("AVAILABLE QUESTS");
        ImGui::SameLine();
        {
            const float bw = 78.0f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                                 ImGui::GetContentRegionAvail().x - bw);
            if (ImGui::Button("Refresh", ImVec2(bw, 0)))
                StartSettingsLoad(ui.load);
        }
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 2));

        ImGui::BeginChild("questlist", ImVec2(0, 0));

        if (!ui.load->done.load())
        {
            ImGui::Dummy(ImVec2(0, 6));
            ImGui::TextColored(COL_MUTED, "Fetching quest list...");
        }
        else
        {
            const Settings& st = ui.load->result;
            if (!st.ok)
            {
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextColored(COL_RED, "%s", st.error.c_str());
                ImGui::TextColored(COL_MUTED,
                    "You can still launch a quest using a custom path.");
                ImGui::PopTextWrapPos();
                ImGui::Dummy(ImVec2(0, 4));
            }

            for (int i = 0; i < (int)st.games.size(); ++i)
            {
                const GameEntry& g = st.games[i];
                char badge[24];
                std::snprintf(badge, sizeof(badge), "%d min", g.seconds / 60);
                const bool sel = !ui.useCustom && ui.selectedGame == i;
                if (GameCard(i, g.name.c_str(), g.path.c_str(), badge, sel))
                {
                    ui.useCustom    = false;
                    ui.selectedGame = i;
                    ui.seconds      = g.seconds;
                }
            }

            if (GameCard(0x7FFF, "Custom path",
                         "Launch a game that is not on the list", nullptr,
                         ui.useCustom))
            {
                ui.useCustom    = true;
                ui.selectedGame = -1;
            }
        }

        ImGui::EndChild();
    }

    bool DrawQuestDetails(CompleterUI& ui, std::string& outName, std::string& outPath)
    {
        SmallCaps("QUEST DETAILS");
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 4));

        bool haveSelection = false;

        if (ui.useCustom)
        {
            ImGui::TextColored(COL_MUTED, "Game name");
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##cname", "e.g. My Game",
                                     ui.customName, sizeof(ui.customName));
            ImGui::Dummy(ImVec2(0, 3));
            ImGui::TextColored(COL_MUTED, "Path (Steam-relative, or a full path)");
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##cpath", "My Game\\game.exe  or  C:\\...\\game.exe",
                                     ui.customPath, sizeof(ui.customPath));

            outName = ui.customName[0] ? ui.customName : "Custom game";
            outPath = ui.customPath;
            haveSelection = ui.customPath[0] != '\0';
        }
        else if (ui.load->done.load() && ui.selectedGame >= 0 &&
                 ui.selectedGame < (int)ui.load->result.games.size())
        {
            const GameEntry& g = ui.load->result.games[ui.selectedGame];
            outName = g.name;
            outPath = g.path;
            haveSelection = true;

            ImGui::BeginChild("selbox", ImVec2(0, 66), ImGuiChildFlags_Borders);
            ImGui::PushFont(g_fontSemibold, 19.0f);
            ImGui::TextUnformatted(g.name.c_str());
            ImGui::PopFont();
            ImGui::PushFont(g_fontUI, 13.0f);
            ImGui::TextColored(COL_MUTED, "%s", g.path.c_str());
            ImGui::PopFont();
            ImGui::EndChild();
        }
        else
        {
            ImGui::Dummy(ImVec2(0, 6));
            ImGui::TextColored(COL_MUTED, "Select a quest from the list to begin.");
        }

        if (haveSelection)
        {
            ImGui::Dummy(ImVec2(0, 6));
            ImGui::TextColored(COL_MUTED, "Play duration");
            int minutes = ui.seconds / 60;
            if (minutes < 1) minutes = 1;
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::SliderInt("##dur", &minutes, 1, 120, "%d minutes"))
                ui.seconds = minutes * 60;
        }

        return haveSelection;
    }

    bool DrawCompleterFrame(CompleterUI& ui)
    {
        PumpTasks();

        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::Begin("##root", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);

        const float footerH = ImGui::GetFrameHeight() * 0.55f + 8.0f;
        const float bodyH   = ImGui::GetContentRegionAvail().y - footerH;
        const float leftW   = ImGui::GetContentRegionAvail().x * 0.46f;

        ImGui::BeginChild("left", ImVec2(leftW, bodyH), ImGuiChildFlags_Borders);
        DrawQuestList(ui);
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("right", ImVec2(0, bodyH), ImGuiChildFlags_Borders);
        {
            std::string name, path;
            const bool haveSelection = DrawQuestDetails(ui, name, path);

            ImGui::Dummy(ImVec2(0, 8));

            const bool busy      = haveSelection && IsPathBusy(path);
            const bool canLaunch = haveSelection && !busy;

            ImGui::BeginDisabled(!canLaunch);
            ImGui::PushStyleColor(ImGuiCol_Button,        COL_ACCENT);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL_ACCENT_H);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  COL_ACCENT_A);
            ImGui::PushFont(g_fontSemibold, 17.0f);
            const char* label = busy ? "Quest already running"
                              : !haveSelection ? "Select a quest first"
                                               : "Launch Quest";
            if (ImGui::Button(label, ImVec2(-FLT_MIN, 46)) && canLaunch)
                StartLaunch(name, path, ui.seconds);
            ImGui::PopFont();
            ImGui::PopStyleColor(3);
            ImGui::EndDisabled();

            ImGui::Dummy(ImVec2(0, 10));
            SmallCaps("ACTIVE");
            ImGui::Separator();

            const auto& tasks = ActiveTasks();
            ImGui::BeginChild("tasks", ImVec2(0, 0));
            if (tasks.empty())
            {
                ImGui::Dummy(ImVec2(0, 6));
                ImGui::TextColored(COL_MUTED, "No quests launched yet.");
            }
            else
            {
                for (int i = (int)tasks.size() - 1; i >= 0; --i)
                    DrawTaskRow(*tasks[i]);
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();


        ImGui::End();
        return true;
    }
}

int RunCompleter()
{
    static CompleterUI ui;
    StartSettingsLoad(ui.load);

    RenderOptions opts;
    opts.title     = L"Discord Quest Completer";
    opts.width     = 880;
    opts.height    = 600;
    opts.resizable = true;

    const int rc = RunImGuiWindow(opts, []() { return DrawCompleterFrame(ui); });

    WaitAllTasks();
    return rc;
}

namespace
{
    std::chrono::steady_clock::time_point g_dummyStart;
    long long    g_dummyDurationMs = 0;
    std::string  g_dummyName       = "Loading";
    std::wstring g_dummyTitle;

    void TextCentered(ImFont* font, float size, const ImVec4& col, const char* text)
    {
        ImGui::PushFont(font, size);
        const float w = ImGui::CalcTextSize(text).x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - w) * 0.5f);
        ImGui::TextColored(col, "%s", text);
        ImGui::PopFont();
    }

    bool DrawDummyFrame()
    {
        const auto now = std::chrono::steady_clock::now();
        const long long elapsedMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - g_dummyStart).count();

        float progress = (g_dummyDurationMs > 0)
            ? (float)((double)elapsedMs / (double)g_dummyDurationMs) : 1.0f;
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;

        const long long leftMs = (g_dummyDurationMs > elapsedMs)
            ? g_dummyDurationMs - elapsedMs : 0;

        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::Begin("##dummy", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

        ImGui::Dummy(ImVec2(0, 6));
        TextCentered(g_fontSemibold, 16.0f, COL_MUTED, g_dummyName.c_str());

        ImGui::Dummy(ImVec2(0, 4));
        char pct[16];
        std::snprintf(pct, sizeof(pct), "%d%%", (int)(progress * 100.0f + 0.5f));
        TextCentered(g_fontSemibold, 44.0f, COL_TEXT, pct);

        ImGui::Dummy(ImVec2(0, 6));
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, COL_ACCENT);
        ImGui::ProgressBar(progress, ImVec2(-FLT_MIN, 12.0f), "");
        ImGui::PopStyleColor();

        ImGui::Dummy(ImVec2(0, 6));
        const std::string left =
            FormatDuration((int)((leftMs + 999) / 1000)) + " left";
        TextCentered(g_fontUI, 14.0f, COL_MUTED, left.c_str());

        ImGui::End();

        return elapsedMs < g_dummyDurationMs;
    }
}

int RunDummy(long long durationMs)
{
    g_dummyDurationMs = durationMs > 0 ? durationMs : 1000;
    g_dummyStart      = std::chrono::steady_clock::now();

    wchar_t buf[MAX_PATH];
    const DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::filesystem::path self(std::wstring(buf, n));
    std::wstring folder = self.parent_path().filename().wstring();
    if (folder.empty())
        folder = L"Loading";
    g_dummyTitle = folder;
    g_dummyName  = WideToUtf8(folder);

    RenderOptions opts;
    opts.title     = g_dummyTitle.c_str();
    opts.width     = 460;
    opts.height    = 210;
    opts.resizable = false;

    return RunImGuiWindow(opts, DrawDummyFrame);
}
