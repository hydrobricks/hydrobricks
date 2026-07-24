#include "Log.h"

#include <fstream>
#include <iostream>
#include <mutex>
#include <string>

namespace {

struct LogState {
    LogLevel level = LogLevel::Message;
    std::ofstream logFile;
    std::mutex mutex;
};

LogState& GetState() {
    static LogState state;
    return state;
}

void WriteToTargets(const std::string& prefix, const std::string& msg) {
    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);

    std::string line = prefix + msg + "\n";
    std::cerr << line;
    if (state.logFile.is_open()) {
        state.logFile << line;
    }
}

}  // namespace

void LogSetLevel(LogLevel level) {
    GetState().level = level;
}

void LogSetFile(const std::string& path) {
    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.logFile.is_open()) {
        state.logFile.close();
    }
    state.logFile.open(path, std::ios::out | std::ios::trunc);
}

void LogFlush() {
    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    std::cerr.flush();
    if (state.logFile.is_open()) {
        state.logFile.flush();
        state.logFile.close();
    }
}

void LogError(const std::string& msg) {
    WriteToTargets("[Error] ", msg);
}

void LogWarning(const std::string& msg) {
    if (GetState().level >= LogLevel::Warning) {
        WriteToTargets("[Warning] ", msg);
    }
}

void LogMessage(const std::string& msg) {
    if (GetState().level >= LogLevel::Message) {
        WriteToTargets("[Info] ", msg);
    }
}

void LogDebug(const std::string& msg) {
    if (GetState().level >= LogLevel::Debug) {
        WriteToTargets("[Debug] ", msg);
    }
}
