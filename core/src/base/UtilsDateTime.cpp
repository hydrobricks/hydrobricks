#include "UtilsDateTime.h"

#include <cmath>
#include <iomanip>
#include <sstream>

#include "TimeConstants.h"


using namespace std::chrono;

std::chrono::sys_days UtilsDateTime::ToSysDays(int year, int month, int day) {
    using year_t = std::chrono::year;
    using month_t = std::chrono::month;
    using day_t = std::chrono::day;

    auto y = year_t{year};
    auto m = month_t{static_cast<unsigned>(month)};
    auto d = day_t{static_cast<unsigned>(day)};
    if (!y.ok() || !m.ok() || !d.ok()) {
        throw InvalidArgument("Invalid Y-M-D provided to ToSysDays");
    }
    std::chrono::year_month_day ymd{y, m, d};
    if (!ymd.ok()) {
        throw InvalidArgument("Invalid calendar date provided to ToSysDays");
    }
    return sys_days{ymd};
}

void UtilsDateTime::ValidateHMS(int hour, int minute, int second) {
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 60) {
        // allow leap second 60
        throw InvalidArgument("Invalid time-of-day (H:M:S)");
    }
}

static double SysTimeToMjd(sys_time<seconds> tp) {
    auto secs = tp.time_since_epoch().count();
    double days_since_unix = static_cast<double>(secs) / SECONDS_PER_DAY_D;
    return MJD_UNIX_EPOCH + days_since_unix;
}

static sys_time<seconds> MjdToSysTime(double mjd) {
    long double delta_days = static_cast<long double>(mjd) - static_cast<long double>(MJD_UNIX_EPOCH);
    long double delta_secs_ld = delta_days * static_cast<long double>(SECONDS_PER_DAY_D);
    auto delta_secs = static_cast<long long>(std::llround(delta_secs_ld));
    return sys_time<seconds>{seconds{delta_secs}};
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
    std::istringstream iss{s};
    iss.exceptions(std::ios::failbit | std::ios::badbit);
    try {
        std::chrono::sys_time<std::chrono::seconds> tp{};
        iss >> std::chrono::parse(fmt, tp);
        if (!iss.fail()) {
            out = tp;
            return true;
        }
        return false;
    } catch (...) {
        return false;
    }
}

static bool ParseDateOnly(const std::string& s, const char* fmt, std::chrono::sys_days& out) {
    std::istringstream iss{s};
    iss.exceptions(std::ios::failbit | std::ios::badbit);
    try {
        std::chrono::sys_days d{};
        iss >> std::chrono::parse(fmt, d);
        if (!iss.fail()) {
            out = d;
            return true;
        }
        return false;
    } catch (...) {
        return false;
    }
}

double UtilsDateTime::ParseToMJD(const std::string& dateStr, TimeFormat format) {
    using seconds_tp = std::chrono::sys_time<std::chrono::seconds>;
    using days_tp = std::chrono::sys_days;

    const std::string s = dateStr;

    seconds_tp tp{};
    days_tp d{};

    auto try_return_days = [&](const std::initializer_list<const char*> pats) -> bool {
        for (auto fmt : pats) {
            if (ParseDateOnly(s, fmt, d)) {
                tp = seconds_tp{d.time_since_epoch()};
                return true;
            }
        }
        return false;
    };
    auto try_return_seconds = [&](const std::initializer_list<const char*> pats) -> bool {
        for (auto fmt : pats) {
            if (ParseExact(s, fmt, tp)) {
                return true;
            }
        }
        return false;
    };

    switch (format) {
        case ISOdate:
        case YYYY_MM_DD: {
            if (try_return_days({"%F", "%Y.%m.%d", "%Y/%m/%d", "%Y%m%d"})) {
                return SysTimeToMjd(tp);
            }
            break;
        }
        case ISOdateTime:
        case YYYY_MM_DD_hh_mm:
        case YYYY_MM_DD_hh_mm_ss: {
            if (try_return_seconds({"%FT%T", "%F %T", "%F %H:%M", "%F %H:%M:%S", "%Y.%m.%d %H:%M", "%Y.%m.%d %H:%M:%S",
                                    "%Y/%m/%d %H:%M", "%Y/%m/%d %H:%M:%S", "%Y%m%d%H%M%S", "%Y%m%d %H:%M",
                                    "%Y%m%dT%H%M%S"})) {
                return SysTimeToMjd(tp);
            }
            break;
        }
        case DD_MM_YYYY: {
            if (try_return_days({"%d.%m.%Y", "%d/%m/%Y", "%d-%m-%Y", "%d%m%Y"})) {
                return SysTimeToMjd(tp);
            }
            break;
        }
        case DD_MM_YYYY_hh_mm:
        case DD_MM_YYYY_hh_mm_ss: {
            if (try_return_seconds({"%d.%m.%Y %H:%M", "%d.%m.%Y %H:%M:%S", "%d/%m/%Y %H:%M", "%d/%m/%Y %H:%M:%S",
                                    "%d-%m-%Y %H:%M", "%d-%m-%Y %H:%M:%S", "%d%m%Y %H:%M", "%d%m%Y %H:%M:%S"})) {
                return SysTimeToMjd(tp);
            }
            break;
        }
        case guess: {
            const std::initializer_list<const char*> date_time_patterns = {"%FT%T",
                                                                           "%F %T",
                                                                           "%F %H:%M",
                                                                           "%F %H:%M:%S",
                                                                           "%Y.%m.%d %H:%M",
                                                                           "%Y.%m.%d %H:%M:%S",
                                                                           "%Y/%m/%d %H:%M",
                                                                           "%Y/%m/%d %H:%M:%S",
                                                                           "%Y%m%d%H%M%S",
                                                                           "%Y%m%d %H:%M",
                                                                           "%Y%m%dT%H%M%S",
                                                                           "%d.%m.%Y %H:%M",
                                                                           "%d.%m.%Y %H:%M:%S",
                                                                           "%d/%m/%Y %H:%M",
                                                                           "%d/%m/%Y %H:%M:%S",
                                                                           "%d-%m-%Y %H:%M",
                                                                           "%d-%m-%Y %H:%M:%S",
                                                                           "%d%m%Y %H:%M",
                                                                           "%d%m%Y %H:%M:%S"};
            const std::initializer_list<const char*> date_only_patterns = {
                "%F", "%Y.%m.%d", "%Y/%m/%d", "%Y%m%d", "%d.%m.%Y", "%d/%m/%Y", "%d-%m-%Y", "%d%m%Y"};

            if (try_return_seconds(date_time_patterns)) {
                return SysTimeToMjd(tp);
            }
            if (try_return_days(date_only_patterns)) {
                return SysTimeToMjd(tp);
            }
            break;
        }
    }

    throw InvalidArgument(wxString::Format(_("The date (%s) conversion failed. Please check the format"), dateStr));
}
