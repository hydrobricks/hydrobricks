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

// The log file of the object currently working on this thread (see LogScope).
thread_local LogFile* scopedFile = nullptr;

void WriteToTargets(const std::string& prefix, const std::string& msg) {
    std::string line = prefix + msg + "\n";
    if (scopedFile != nullptr && scopedFile->IsOpen()) {
        {
            auto& state = GetState();
            std::lock_guard<std::mutex> lock(state.mutex);
            std::cerr << line;
        }
        scopedFile->Write(line);
        return;
    }

    auto& state = GetState();
    std::lock_guard<std::mutex> lock(state.mutex);
    std::cerr << line;
    if (state.logFile.is_open()) {
        state.logFile << line;
    }
}

}  // namespace

struct LogFile::Impl {
    std::ofstream file;
    std::mutex mutex;
};

LogFile::LogFile()
    : _impl(std::make_unique<Impl>()) {}

LogFile::~LogFile() = default;

void LogFile::Open(const std::string& path) {
    std::lock_guard<std::mutex> lock(_impl->mutex);
    if (_impl->file.is_open()) {
        _impl->file.close();
    }
    _impl->file.open(path, std::ios::out | std::ios::trunc);
}

void LogFile::Close() {
    std::lock_guard<std::mutex> lock(_impl->mutex);
    if (_impl->file.is_open()) {
        _impl->file.flush();
        _impl->file.close();
    }
}

bool LogFile::IsOpen() {
    std::lock_guard<std::mutex> lock(_impl->mutex);
    return _impl->file.is_open();
}

void LogFile::Write(const std::string& line) {
    std::lock_guard<std::mutex> lock(_impl->mutex);
    if (_impl->file.is_open()) {
        _impl->file << line;
    }
}

LogScope::LogScope(LogFile* file)
    : _previous(scopedFile) {
    scopedFile = file;
}

LogScope::~LogScope() {
    scopedFile = _previous;
}

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
