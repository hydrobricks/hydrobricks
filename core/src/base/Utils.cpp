#include "Utils.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

// New centralized date/time utilities
#include "UtilsDateTime.h"

static string GetUserDataDir() {
#ifdef _WIN32
    const char* appdata = getenv("APPDATA");
    if (appdata) return string(appdata);
    const char* home = getenv("USERPROFILE");
    if (home) return string(home);
    return ".";
#else
    const char* home = getenv("HOME");
    if (home) return string(home) + "/.local/share";
    return ".";
#endif
}

string GetUserDirPath() {
    return (std::filesystem::path(GetUserDataDir()) / "hydrobricks").string();
}

bool InitHydrobricks() {
    std::error_code ec;
    std::filesystem::create_directories(GetUserDirPath(), ec);
    return !ec;
}

bool InitHydrobricksForPython() {
    return InitHydrobricks();
}

void InitLog(const string& path) {
    auto now = std::chrono::system_clock::now();
    auto nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&nowTime);
    char timeBuf[32];
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d_%H%M%S", tm);

    string logFileName = std::format("hydrobricks_{}.log", timeBuf);
    string fullPath = (std::filesystem::path(path) / logFileName).string();
    LogSetFile(fullPath);

    string version = std::format("{}.{}.{}", HYDROBRICKS_MAJOR_VERSION, HYDROBRICKS_MINOR_VERSION,
                                 HYDROBRICKS_PATCH_VERSION);
    LogMessage(std::format("hydrobricks version {}, {}", version, __DATE__));
}

void CloseLog() {
    LogFlush();
}

void SetMaxLogLevel() {
    LogSetLevel(LogLevel::Debug);
}

void SetDebugLogLevel() {
    LogSetLevel(LogLevel::Debug);
}

void SetMessageLogLevel() {
    LogSetLevel(LogLevel::Message);
}

bool CheckOutputDirectory(const string& path) {
    if (!std::filesystem::is_directory(path)) {
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        if (ec) {
            LogError("The path {} could not be created.", path);
            return false;
        }
    }

    return true;
}

void DisplayProcessingTime(std::chrono::steady_clock::time_point startTime) {
    auto elapsed = std::chrono::steady_clock::now() - startTime;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    float dispTime = static_cast<float>(ms);
    string watchUnit = "ms";
    if (dispTime > 3600000) {
        dispTime /= 3600000.0f;
        watchUnit = "h";
    } else if (dispTime > 60000) {
        dispTime /= 60000.0f;
        watchUnit = "min";
    } else if (dispTime > 1000) {
        dispTime /= 1000.0f;
        watchUnit = "s";
    }
    LogMessage(std::format("Processing time: {:.2f} {}.", dispTime, watchUnit));
}

bool IsNaN(const int value) {
    return value == INT_NAN_SENTINEL;
}

bool IsNaN(const float value) {
    return value != value;
}

bool IsNaN(const double value) {
    return value != value;
}

const char* GetPathSeparator() {
    return "/";
}

bool StringsMatch(const string& str1, const string& str2) {
    if (str1.size() != str2.size()) {
        return false;
    }

    return std::equal(str1.begin(), str1.end(), str2.begin(), [](const char& c1, const char& c2) {
        return (c1 == c2 ||
                std::toupper(static_cast<unsigned char>(c1)) == std::toupper(static_cast<unsigned char>(c2)));
    });
}

int Find(const int* start, const int* end, int value, int tolerance, bool showWarning) {
    return FindT<int>(start, end, value, tolerance, showWarning);
}

int Find(const float* start, const float* end, float value, float tolerance, bool showWarning) {
    return FindT<float>(start, end, value, tolerance, showWarning);
}

int Find(const double* start, const double* end, double value, double tolerance, bool showWarning) {
    return FindT<double>(start, end, value, tolerance, showWarning);
}

template <class T>
int FindT(const T* start, const T* end, T value, T tolerance, bool showWarning) {
    assert(start);
    assert(end);
    assert(start <= end);

    // Convert from inclusive [start, end] to standard [start, end) range
    const T* searchEnd = end + 1;

    // Handle single-element or multi-element array
    if (start == end) {
        // Single element array - check if it matches
        if (NearlyEqual(value, *start, tolerance)) {
            return 0;
        }
        if (showWarning) {
            LogWarning("The value was not found in the array.");
        }
        return NOT_FOUND;
    }

    // Determine if array is sorted ascending or descending
    bool ascending = *end > *start;

    // Use std::lower_bound for binary search
    const T* it;
    if (ascending) {
        it = std::lower_bound(start, searchEnd, value);
    } else {
        // For descending order, use reverse comparator
        it = std::lower_bound(start, searchEnd, value, [](const T& a, const T& b) { return a > b; });
    }

    // Check if exact match or within tolerance at current position
    if (it != searchEnd && NearlyEqual(value, *it, tolerance)) {
        return static_cast<int>(it - start);
    }

    // Check previous element if available
    if (it != start) {
        const T* prev = it - 1;
        if (NearlyEqual(value, *prev, tolerance)) {
            return static_cast<int>(prev - start);
        }
    }

    // Check if value is within array bounds
    if (ascending) {
        if (value < *start || value > *end) {
            if (showWarning) {
                LogWarning("The value ({}) is out of the array range.", static_cast<float>(value));
            }
            return OUT_OF_RANGE;
        }
    } else {
        if (value > *start || value < *end) {
            if (showWarning) {
                LogWarning("The value ({}) is out of the array range.", static_cast<float>(value));
            }
            return OUT_OF_RANGE;
        }
    }

    if (showWarning) {
        LogWarning("The value was not found in the array.");
    }
    return NOT_FOUND;
}

double IncrementDateBy(double date, int amount, TimeUnit unit) {
    switch (unit) {
        case Week:
            return date + amount * 7;
        case Day:
            return date + amount;
        case Hour:
            return date + amount / 24.0;
        case Minute:
            return date + amount / 1440.0;
        default:
            LogError("The provided time step unit is not allowed.");
    }

    return 0;
}

Time GetTimeStructFromMJD(double mjd) {
    assert(mjd >= 0);

    return UtilsDateTime::FromMJD(mjd);
}

double ParseDate(const string& dateStr, TimeFormat format) {
    return UtilsDateTime::ParseToMJD(static_cast<const std::string&>(dateStr), format);
}

double GetMJD(int year, int month, int day, int hour, int minute, int second) {
    assert(year > 0);
    assert(month > 0);
    assert(day > 0);
    assert(hour >= 0);
    assert(minute >= 0);
    assert(second >= 0);

    return UtilsDateTime::ToMJD(year, month, day, hour, minute, second);
}
