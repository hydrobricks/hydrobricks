#ifndef HYDROBRICKS_LOG_H
#define HYDROBRICKS_LOG_H

#include <format>
#include <string>

enum class LogLevel {
    Error,
    Warning,
    Message,
    Debug
};

/**
 * Set the minimum log level. Messages below this level are suppressed.
 * Default: LogLevel::Message (Error, Warning, and Message are shown).
 */
void LogSetLevel(LogLevel level);

/**
 * Open a log file at the given path (in addition to stderr output).
 */
void LogSetFile(const std::string& path);

/**
 * Flush and close the log file.
 */
void LogFlush();

// Core logging functions (non-template)
void LogError(const std::string& msg);
void LogWarning(const std::string& msg);
void LogMessage(const std::string& msg);
void LogDebug(const std::string& msg);

// Template overloads for formatted messages (std::format style)
template <typename... Args>
void LogError(std::format_string<Args...> fmt, Args&&... args) {
    LogError(std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void LogWarning(std::format_string<Args...> fmt, Args&&... args) {
    LogWarning(std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void LogMessage(std::format_string<Args...> fmt, Args&&... args) {
    LogMessage(std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void LogDebug(std::format_string<Args...> fmt, Args&&... args) {
    LogDebug(std::format(fmt, std::forward<Args>(args)...));
}

#endif  // HYDROBRICKS_LOG_H
