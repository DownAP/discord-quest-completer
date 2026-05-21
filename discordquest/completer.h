#pragma once
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

enum class TaskState
{
    Running,
    Finished,
    Stopped,
    Failed,
};

struct LaunchTask
{
    std::string                           name;
    std::wstring                          fullPath;
    int                                   durationSeconds = 0;
    std::chrono::steady_clock::time_point  startTime;
    std::atomic<TaskState>                state{ TaskState::Running };
    std::atomic<bool>                     stopRequested{ false };
    std::atomic<float>                    stoppedAt{ -1.0f };
    std::thread                           worker;

    float Progress() const;
    int SecondsLeft() const;
    void RequestStop();
    void SetMessage(std::string m);
    std::string Message() const;

private:
    mutable std::mutex msgMutex;
    std::string        message{ "Starting..." };
};

std::wstring SteamCommonPath();

std::shared_ptr<LaunchTask> StartLaunch(const std::string& name,
                                        const std::string& relPath,
                                        int seconds);

void PumpTasks();
const std::vector<std::shared_ptr<LaunchTask>>& ActiveTasks();
void WaitAllTasks();

bool IsPathBusy(const std::string& relPath);
