#include "UtilsDateTime.h"

#include <cctype>
#include <cmath>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <locale>
#include <sstream>

#include "TimeConstants.h"

using namespace std::chrono;

// Platform-specific date parsing helper
#ifdef _WIN32
// Windows implementation using get_time
static bool PlatformParseTm(const std::string& s, const char* fmt, std::tm& tm) {
    std::istringstream iss(s);
    iss.imbue(std::locale("C"));  // Use C locale for consistent parsing
    iss >> std::get_time(&tm, fmt);
    if (iss.fail()) {
        return false;
    }
    // Verify the entire string was consumed
    iss >> std::ws;  // skip whitespace
    return iss.eof() || iss.peek() == EOF;
}
#else
// Unix/Linux implementation using strptime
static bool PlatformParseTm(const std::string& s, const char* fmt, std::tm& tm) {
    std::memset(&tm, 0, sizeof(tm));
    tm.tm_isdst = -1;  // Let mktime determine DST
    const char* result = strptime(s.c_str(), fmt, &tm);
    if (result == nullptr) {
        return false;
    }
    // Check if the entire string was consumed (or only whitespace remains)
    while (*result && std::isspace(static_cast<unsigned char>(*result))) {
        ++result;
    }
    return *result == '\0';  // True if we consumed entire string
}
#endif

sys_days UtilsDateTime::ToSysDays(int year, int month, int day) {
    using yearT = std::chrono::year;
    using monthT = std::chrono::month;
    using dayT = std::chrono::day;

    auto y = yearT{year};
    auto m = monthT{static_cast<unsigned>(month)};
    auto d = dayT{static_cast<unsigned>(day)};
    if (!y.ok() || !m.ok() || !d.ok()) {
        throw RuntimeError("Invalid Y-M-D provided to ToSysDays");
    }
    std::chrono::year_month_day ymd{y, m, d};
    if (!ymd.ok()) {
        throw RuntimeError("Invalid calendar date provided to ToSysDays");
    }
    return sys_days{ymd};
}

void UtilsDateTime::ValidateHMS(int hour, int minute, int second) {
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 60) {
        // allow leap second 60
        throw RuntimeError("Invalid time-of-day (H:M:S)");
    }
}

static double SysTimeToMjd(sys_time<seconds> tp) {
    auto secs = tp.time_since_epoch().count();
    double daysSinceUnix = static_cast<double>(secs) / SECONDS_PER_DAY_D;
    return MJD_UNIX_EPOCH + daysSinceUnix;
}

static sys_time<seconds> MjdToSysTime(double mjd) {
    long double deltaDays = static_cast<long double>(mjd) - static_cast<long double>(MJD_UNIX_EPOCH);
    long double deltaSecsLd = deltaDays * static_cast<long double>(SECONDS_PER_DAY_D);
    auto deltaSecs = static_cast<long long>(std::llround(deltaSecsLd));
    return sys_time<seconds>{seconds{deltaSecs}};
}

double UtilsDateTime::ToMJD(int year, int month, int day, int hour, int minute, int second) {
    ValidateHMS(hour, minute, second);
    auto sd = ToSysDays(year, month, day);
    auto tp = sys_time<seconds>{sd.time_since_epoch()} + hours{hour} + minutes{minute} + seconds{second};
    return SysTimeToMjd(tp);
}

Time UtilsDateTime::FromMJD(double mjd) {
    auto tp = MjdToSysTime(mjd);
    auto sd = floor<days>(tp);
    auto time_of_day = tp - sd;

    year_month_day ymd{floor<days>(tp)};
    Time t{};
    t.year = int(ymd.year());
    t.month = unsigned(ymd.month());
    t.day = unsigned(ymd.day());

    auto h = duration_cast<hours>(time_of_day).count();
    auto m = duration_cast<minutes>(time_of_day - hours{h}).count();
    auto s = duration_cast<seconds>(time_of_day - hours{h} - minutes{m}).count();
    t.hour = static_cast<int>(h);
    t.min = static_cast<int>(m);
    t.sec = static_cast<int>(s);
    return t;
}

static bool ParseExact(const std::string& s, const char* fmt, std::chrono::sys_time<std::chrono::seconds>& out) {
    try {
        std::tm tm{};
        tm.tm_isdst = -1;  // Let mktime determine DST
        if (!PlatformParseTm(s, fmt, tm)) {
            return false;
        }

        // Convert tm to sys_time<seconds>
        // Build the date part using chrono types to avoid timezone issues
        auto parsed_year = static_cast<int>(tm.tm_year) + 1900;
        auto parsed_month = static_cast<unsigned>(tm.tm_mon) + 1;
        auto parsed_day = static_cast<unsigned>(tm.tm_mday);
        auto parsed_hour = static_cast<int>(tm.tm_hour);
        auto parsed_min = static_cast<int>(tm.tm_min);
        auto parsed_sec = static_cast<int>(tm.tm_sec);

        auto ymd = std::chrono::year{parsed_year} / std::chrono::month{parsed_month} / std::chrono::day{parsed_day};
        auto days = std::chrono::sys_days{ymd};
        auto tp = std::chrono::time_point_cast<std::chrono::seconds>(days) + std::chrono::hours{parsed_hour} +
                  std::chrono::minutes{parsed_min} + std::chrono::seconds{parsed_sec};
        out = tp;
        return true;
    } catch (...) {
        return false;
    }
}

static bool ParseDateOnly(const std::string& s, const char* fmt, std::chrono::sys_days& out) {
    try {
        std::tm tm{};
        tm.tm_isdst = -1;  // Let mktime determine DST

        if (!PlatformParseTm(s, fmt, tm)) {
            return false;
        }

        // Convert tm to sys_days
        // Use the parsed values directly to construct year_month_day
        auto parsed_year = static_cast<int>(tm.tm_year) + 1900;    // tm_year is years since 1900
        auto parsed_month = static_cast<unsigned>(tm.tm_mon) + 1;  // tm_mon is 0-based
        auto parsed_day = static_cast<unsigned>(tm.tm_mday);       // tm_mday is 1-based

        auto ymd = std::chrono::year{parsed_year} / std::chrono::month{parsed_month} / std::chrono::day{parsed_day};
        out = std::chrono::sys_days{ymd};
        return true;
    } catch (...) {
        return false;
    }
}

double UtilsDateTime::ParseToMJD(const std::string& dateStr, TimeFormat format) {
    using secondsTp = sys_time<std::chrono::seconds>;
    using daysTp = std::chrono::sys_days;

    const std::string s = dateStr;

    secondsTp tp{};
    daysTp d{};

    auto TryReturnDays = [&](const std::initializer_list<const char*> pats) -> bool {
        for (auto fmt : pats) {
            if (ParseDateOnly(s, fmt, d)) {
                tp = secondsTp{d.time_since_epoch()};
                return true;
            }
        }
        return false;
    };
    auto TryReturnSeconds = [&](const std::initializer_list<const char*> pats) -> bool {
        for (auto fmt : pats) {
            if (ParseExact(s, fmt, tp)) {
                return true;
            }
        }
        return false;
    };

    // Helper: validate that a parsed date has all valid components
    // Checks: year is reasonable [1900-2100], month is 1-12, day is 1-31
    auto IsValidDate = [](const std::chrono::year_month_day& ymd) -> bool {
        int y = int(ymd.year());
        unsigned m = static_cast<unsigned>(ymd.month());
        unsigned d = static_cast<unsigned>(ymd.day());

        // Year range: reject obviously invalid years (e.g., year 23 or 2007 when parsed as day)
        if (y < 1900 || y > 2100) {
            return false;
        }
        // Month must be 1-12
        if (m < 1 || m > 12) {
            return false;
        }
        // Day must be 1-31 (chrono will have already validated max day per month)
        if (d < 1 || d > 31) {
            return false;
        }
        return true;
    };

    switch (format) {
        case ISOdate:
        case YYYY_MM_DD: {
            if (TryReturnDays({"%Y-%m-%d", "%Y.%m.%d", "%Y/%m/%d", "%Y%m%d"})) {
                year_month_day ymd{floor<days>(d)};
                if (IsValidDate(ymd)) {
                    return SysTimeToMjd(tp);
                }
            }
            break;
        }
        case ISOdateTime:
        case YYYY_MM_DD_hh_mm:
        case YYYY_MM_DD_hh_mm_ss: {
            if (TryReturnSeconds({"%Y-%m-%dT%H:%M:%S", "%Y-%m-%d %H:%M:%S", "%Y.%m.%d %H:%M:%S", "%Y.%m.%d %H:%M",
                                  "%Y-%m-%d %H:%M:%S", "%Y-%m-%d %H:%M", "%Y/%m/%d %H:%M:%S", "%Y/%m/%d %H:%M",
                                  "%Y%m%d %H:%M", "%Y%m%d %H%M%S", "%Y%m%d %H%M", "%Y%m%d%H%M%S", "%Y%m%dT%H%M%S"})) {
                year_month_day ymd{floor<days>(tp)};
                if (IsValidDate(ymd)) {
                    return SysTimeToMjd(tp);
                }
            }
            break;
        }
        case DD_MM_YYYY: {
            if (TryReturnDays({"%d.%m.%Y", "%d/%m/%Y", "%d-%m-%Y", "%d%m%Y"})) {
                year_month_day ymd{floor<days>(d)};
                if (IsValidDate(ymd)) {
                    return SysTimeToMjd(tp);
                }
            }
            break;
        }
        case DD_MM_YYYY_hh_mm:
        case DD_MM_YYYY_hh_mm_ss: {
            if (TryReturnSeconds({"%d.%m.%Y %H:%M:%S", "%d.%m.%Y %H:%M", "%d/%m/%Y %H:%M:%S", "%d/%m/%Y %H:%M",
                                  "%d-%m-%Y %H:%M:%S", "%d-%m-%Y %H:%M", "%d%m%Y %H:%M:%S", "%d%m%Y %H:%M",
                                  "%d%m%Y %H%M%S", "%d%m%Y %H%M"})) {
                year_month_day ymd{floor<days>(tp)};
                if (IsValidDate(ymd)) {
                    return SysTimeToMjd(tp);
                }
            }
            break;
        }
        case guess: {
            const std::initializer_list<const char*> date_time_patterns = {
                "%Y-%m-%dT%H:%M:%S", "%Y-%m-%d %H:%M:%S", "%Y.%m.%d %H:%M:%S", "%Y.%m.%d %H:%M",    "%Y-%m-%d %H:%M:%S",
                "%Y-%m-%d %H:%M",    "%Y/%m/%d %H:%M:%S", "%Y/%m/%d %H:%M",    "%Y%m%d%H%M%S",      "%Y%m%d %H:%M",
                "%Y%m%dT%H%M%S",     "%d.%m.%Y %H:%M:%S", "%d.%m.%Y %H:%M",    "%d/%m/%Y %H:%M:%S", "%d/%m/%Y %H:%M",
                "%d-%m-%Y %H:%M:%S", "%d-%m-%Y %H:%M",    "%d%m%Y %H:%M:%S",   "%d%m%Y %H:%M"};
            const std::initializer_list<const char*> date_only_patterns = {"%Y-%m-%d", "%Y/%m/%d", "%Y%m%d", "%d.%m.%Y",
                                                                           "%d/%m/%Y", "%d-%m-%Y", "%d%m%Y"};

            if (TryReturnSeconds(date_time_patterns)) {
                year_month_day ymd{floor<days>(tp)};
                if (IsValidDate(ymd)) {
                    return SysTimeToMjd(tp);
                }
            }
            if (TryReturnDays(date_only_patterns)) {
                year_month_day ymd{floor<days>(tp)};
                if (IsValidDate(ymd)) {
                    return SysTimeToMjd(tp);
                }
            }
            break;
        }
    }

    throw InputError(std::format("The date ({}) conversion failed. Please check the format", dateStr));
}
