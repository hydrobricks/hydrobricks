#ifndef HYDROBRICKS_LOG_H
#define HYDROBRICKS_LOG_H

#include <format>
#include <memory>
#include <string>

enum class LogLevel {
    Error,
    Warning,
    Message,
    Debug
};

/**
 * A log file owned by one object (e.g. one model instance), so that several models living
 * in the same process each keep their own log, whatever the order in which they are
 * created, run and destroyed. Messages reach it through a LogScope.
 */
class LogFile {
  public:
    LogFile();
    ~LogFile();
    LogFile(const LogFile&) = delete;
    LogFile& operator=(const LogFile&) = delete;

    /**
     * Open (and truncate) the file at the given path, closing any previous one.
     */
    void Open(const std::string& path);

    /**
     * Flush and close the file.
     */
    void Close();

    [[nodiscard]] bool IsOpen();

    void Write(const std::string& line);

  private:
    // The stream and its mutex live in the implementation: this header is part of the
    // core's common includes, which must not pull in <fstream> and <mutex>.
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

/**
 * Route the messages logged on the current thread to a log file while the scope is alive
 * (RAII), instead of the global log file. Scopes nest: the previous target is restored on
 * exit. A null or closed file leaves the messages on the global log file.
 */
class LogScope {
  public:
    explicit LogScope(LogFile* file);
    ~LogScope();
    LogScope(const LogScope&) = delete;
    LogScope& operator=(const LogScope&) = delete;

  private:
    LogFile* _previous;
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
