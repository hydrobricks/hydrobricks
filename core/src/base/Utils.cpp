#include "Utils.h"

#include <wx/ffile.h>
#include <wx/fileconf.h>
#include <wx/stdpaths.h>

bool InitHydrobricks() {
    // Create the user directory if necessary
    wxFileName userDir = wxFileName::DirName(GetUserDirPath());
    if (!userDir.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
        return false;
    }

    // Set config file
    wxFileName filePath = wxFileConfig::GetLocalFile("hydrobricks", wxCONFIG_USE_LOCAL_FILE | wxCONFIG_USE_SUBDIR);
    filePath.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    wxFileConfig* pConfig = new wxFileConfig("hydrobricks", wxEmptyString, filePath.GetFullPath(), wxEmptyString,
                                             wxCONFIG_USE_LOCAL_FILE | wxCONFIG_USE_SUBDIR);
    wxFileConfig::Set(pConfig);

    return true;
}

bool InitHydrobricksForPython() {
    wxInitialize();
    return InitHydrobricks();
}

wxString GetUserDirPath() {
    wxStandardPathsBase& stdPth = wxStandardPaths::Get();
    stdPth.UseAppInfo(0);
    wxString userDir = stdPth.GetUserDataDir();
    userDir.Append(wxFileName::GetPathSeparator());
    userDir.Append("hydrobricks");
    userDir.Append(wxFileName::GetPathSeparator());

    return userDir;
}

void InitLog(const string& path) {
    wxString fullPath = path;
    wxString logFileName = wxString::Format("hydrobricks_%s.log", wxDateTime::Now().Format("%Y-%m-%d_%H%M%S"));
    fullPath.Append(wxFileName::GetPathSeparator());
    fullPath.Append(logFileName);
    wxFFile* logFile = new wxFFile(fullPath, "w");
    auto* pLogFile = new wxLogStderr(logFile->fp());
    new wxLogChain(pLogFile);
    wxString version = wxString::Format("%d.%d.%d", HYDROBRICKS_MAJOR_VERSION, HYDROBRICKS_MINOR_VERSION,
                                        HYDROBRICKS_PATCH_VERSION);
    wxLogMessage("hydrobricks version %s, %s", version, static_cast<const wxChar*>(wxString::FromAscii(__DATE__)));
}

void CloseLog() {
    wxLog::FlushActive();
    delete wxLog::SetActiveTarget(nullptr);
}

void SetMaxLogLevel() {
    wxLog::SetLogLevel(wxLOG_Max);
}

void SetDebugLogLevel() {
    wxLog::SetLogLevel(wxLOG_Debug);
}

void SetMessageLogLevel() {
    wxLog::SetLogLevel(wxLOG_Message);
}

bool CheckOutputDirectory(const string& path) {
    if (!wxFileName::DirExists(path)) {
        wxFileName dir = wxFileName(path);
        if (!dir.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
            wxLogError(_("The path %s could not be created."), path);
            return false;
        }
    }

    return true;
}

void DisplayProcessingTime(const wxStopWatch& sw) {
    auto dispTime = float(sw.Time());
    wxString watchUnit = "ms";
    if (dispTime > 3600000) {
        dispTime = dispTime / 3600000.0f;
        watchUnit = "h";
    } else if (dispTime > 60000) {
        dispTime = dispTime / 60000.0f;
        watchUnit = "min";
    } else if (dispTime > 1000) {
        dispTime = dispTime / 1000.0f;
        watchUnit = "s";
    }
    wxLogMessage(_("Processing time: %.2f %s."), dispTime, watchUnit);
}

bool IsNaN(const int value) {
    return value == NAN_I;
}

bool IsNaN(const float value) {
    return value != value;
}

bool IsNaN(const double value) {
    return value != value;
}

const char* GetPathSeparator() {
    return wxString(wxFileName::GetPathSeparator()).c_str();
}

bool StringsMatch(const string& str1, const string& str2) {
    return ((str1.size() == str2.size()) &&
            std::equal(str1.begin(), str1.end(), str2.begin(), [](const char& c1, const char& c2) {
                return (c1 == c2 || std::toupper(c1) == std::toupper(c2));
            }));
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
    wxASSERT(start);
    wxASSERT(end);

    T *first = nullptr, *mid = nullptr, *last = nullptr;
    int length;

    // Initialize first and last variables.
    first = const_cast<T*>(start);
    last = const_cast<T*>(end);

    // Check array order
    if (*last > *first) {
        // Binary search
        while (first <= last) {
            length = (int)(last - first);
            mid = first + length / 2;
            if (value - tolerance > *mid) {
                first = mid + 1;
            } else if (value + tolerance < *mid) {
                last = mid - 1;
            } else {
                // Return found index
                return static_cast<int>(mid - start);
            }
        }

        // Check the pointers
        if (last - start < 0) {
            last = const_cast<T*>(start);
        } else if (last - end > 0) {
            last = const_cast<T*>(end) - 1;
        } else if (last - end == 0) {
            last -= 1;
        }

        // If the value was not found, return closest value inside tolerance
        if (std::abs(value - *last) <= std::abs(value - *(last + 1))) {
            if (std::abs(value - *last) <= tolerance) {
                return static_cast<int>(last - start);
            } else {
                // Check that the value is within the array. Do it here to allow a margin for the tolerance
                if (value > *end || value < *start) {
                    if (showWarning) {
                        wxLogWarning(_("The value (%f) is out of the array range."), float(value));
                    }
                    return OUT_OF_RANGE;
                }
                if (showWarning) {
                    wxLogWarning(_("The value was not found in the array."));
                }
                return NOT_FOUND;
            }
        } else {
            if (std::abs(value - *(last + 1)) <= tolerance) {
                return static_cast<int>(last - start + 1);
            } else {
                // Check that the value is within the array. Do it here to allow a margin for the tolerance
                if (value > *end || value < *start) {
                    if (showWarning) {
                        wxLogWarning(_("The value (%f) is out of the array range."), float(value));
                    }
                    return OUT_OF_RANGE;
                }
                if (showWarning) {
                    wxLogWarning(_("The value was not found in the array."));
                }
                return NOT_FOUND;
            }
        }
    } else if (*last < *first) {
        // Binary search
        while (first <= last) {
            length = static_cast<int>(last - first);
            mid = first + length / 2;
            if (value - tolerance > *mid) {
                last = mid - 1;
            } else if (value + tolerance < *mid) {
                first = mid + 1;
            } else {
                // Return found index
                return static_cast<int>(mid - start);
            }
        }

        // Check the pointers
        if (first - start < 0) {
            first = const_cast<T*>(start) + 1;
        } else if (first - end > 0) {
            first = const_cast<T*>(end);
        } else if (first - start == 0) {
            first += 1;
        }

        // If the value was not found, return closest value inside tolerance
        if (std::abs(value - *first) <= std::abs(value - *(first - 1))) {
            if (std::abs(value - *first) <= tolerance) {
                return static_cast<int>(first - start);
            } else {
                // Check that the value is within the array. Do it here to allow a margin for the tolerance.
                if (value < *end || value > *start) {
                    if (showWarning) {
                        wxLogWarning(_("The value (%f) is out of the array range."), float(value));
                    }
                    return OUT_OF_RANGE;
                }
                if (showWarning) {
                    wxLogWarning(_("The value was not found in the array."));
                }
                return NOT_FOUND;
            }
        } else {
            if (std::abs(value - *(first - 1)) <= tolerance) {
                return static_cast<int>(first - start - 1);
            } else {
                // Check that the value is within the array. Do it here to allow a margin for the tolerance.
                if (value < *end || value > *start) {
                    if (showWarning) {
                        wxLogWarning(_("The value (%f) is out of the array range."), float(value));
                    }
                    return OUT_OF_RANGE;
                }
                if (showWarning) {
                    wxLogWarning(_("The value was not found in the array."));
                }
                return NOT_FOUND;
            }
        }
    } else {
        if (last - first == 0) {
            if (*first >= value - tolerance && *first <= value + tolerance) {
                return 0;  // Value corresponds
            } else {
                return OUT_OF_RANGE;
            }
        }

        if (*first >= value - tolerance && *first <= value + tolerance) {
            return 0;  // Value corresponds
        } else {
            return OUT_OF_RANGE;
        }
    }
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
            wxLogError(_("The provided time step unit is not allowed."));
    }

    return 0;
}

Time GetTimeStructFromMJD(double mjd) {
    wxASSERT(mjd >= 0);

    // To Julian day
    double jd = mjd + 2400001;  // And not 2400000.5 (not clear why...)

    Time date;

    // Remaining seconds
    double rest = jd - floor(jd);
    double sec = round(rest * 86400);
    date.hour = (int)floor((float)(sec / 3600));
    sec -= date.hour * 3600;
    date.min = (int)floor((float)(sec / 60));
    sec -= date.min * 60;
    date.sec = (int)sec;

    long a, b, c, d, e, z;

    z = (long)jd;
    if (z < 2299161L)
        a = z;
    else {
        auto alpha = (long)((z - 1867216.25) / 36524.25);
        a = z + 1 + alpha - alpha / 4;
    }
    b = a + 1524;
    c = (long)((b - 122.1) / 365.25);
    d = (long)(365.25 * c);
    e = (long)((b - d) / 30.6001);
    date.day = (int)b - d - (long)(30.6001 * e);
    date.month = (int)(e < 13.5) ? e - 1 : e - 13;
    date.year = (int)(date.month > 2.5) ? (c - 4716) : c - 4715;
    if (date.year <= 0) date.year -= 1;

    return date;
}

double ParseDate(const string& dateStr, TimeFormat format) {
    long day = 0;
    long month = 0;
    long year = 0;
    long hour = 0;
    long min = 0;
    long sec = 0;

    wxString date = wxString(dateStr);

    switch (format) {
        case (ISOdate):

            if (date.Len() == 10) {
                if (date.Mid(0, 4).ToLong(&year) && date.Mid(5, 2).ToLong(&month) && date.Mid(8, 2).ToLong(&day)) {
                    return GetMJD(year, month, day);
                }
            }
            break;

        case (ISOdateTime):

            if (date.Len() == 19) {
                if (date.Mid(0, 4).ToLong(&year) && date.Mid(5, 2).ToLong(&month) && date.Mid(8, 2).ToLong(&day) &&
                    date.Mid(11, 2).ToLong(&hour) && date.Mid(14, 2).ToLong(&min) && date.Mid(17, 2).ToLong(&sec)) {
                    return GetMJD(year, month, day, hour, min, sec);
                }
            }
            break;

        case (DD_MM_YYYY):

            if (date.Len() == 10) {
                if (date.Mid(0, 2).ToLong(&day) && date.Mid(3, 2).ToLong(&month) && date.Mid(6, 4).ToLong(&year)) {
                    return GetMJD(year, month, day);
                }
            } else if (date.Len() == 8) {
                if (date.Mid(0, 2).ToLong(&day) && date.Mid(2, 2).ToLong(&month) && date.Mid(4, 4).ToLong(&year)) {
                    return GetMJD(year, month, day);
                }
            }
            break;

        case (YYYY_MM_DD):

            if (date.Len() == 10) {
                if (date.Mid(0, 4).ToLong(&year) && date.Mid(5, 2).ToLong(&month) && date.Mid(8, 2).ToLong(&day)) {
                    return GetMJD(year, month, day);
                }
            } else if (date.Len() == 8) {
                if (date.Mid(0, 4).ToLong(&year) && date.Mid(4, 2).ToLong(&month) && date.Mid(6, 2).ToLong(&day)) {
                    return GetMJD(year, month, day);
                }
            }
            break;

        case (DD_MM_YYYY_hh_mm):

            if (date.Len() == 16) {
                if (date.Mid(0, 2).ToLong(&day) && date.Mid(3, 2).ToLong(&month) && date.Mid(6, 4).ToLong(&year) &&
                    date.Mid(11, 2).ToLong(&hour) && date.Mid(14, 2).ToLong(&min)) {
                    return GetMJD(year, month, day, hour, min);
                }
            } else if (date.Len() == 13) {
                if (date.Mid(0, 2).ToLong(&day) && date.Mid(2, 2).ToLong(&month) && date.Mid(4, 4).ToLong(&year) &&
                    date.Mid(9, 2).ToLong(&hour) && date.Mid(11, 2).ToLong(&min)) {
                    return GetMJD(year, month, day, hour, min);
                }
            }
            break;

        case (YYYY_MM_DD_hh_mm):

            if (date.Len() == 16) {
                if (date.Mid(0, 4).ToLong(&year) && date.Mid(5, 2).ToLong(&month) && date.Mid(8, 2).ToLong(&day) &&
                    date.Mid(11, 2).ToLong(&hour) && date.Mid(14, 2).ToLong(&min)) {
                    return GetMJD(year, month, day, hour, min);
                }
            } else if (date.Len() == 13) {
                if (date.Mid(0, 4).ToLong(&year) && date.Mid(4, 2).ToLong(&month) && date.Mid(6, 2).ToLong(&day) &&
                    date.Mid(9, 2).ToLong(&hour) && date.Mid(11, 2).ToLong(&min)) {
                    return GetMJD(year, month, day, hour, min);
                }
            }
            break;

        case (DD_MM_YYYY_hh_mm_ss):

            if (date.Len() == 19) {
                if (date.Mid(0, 2).ToLong(&day) && date.Mid(3, 2).ToLong(&month) && date.Mid(6, 4).ToLong(&year) &&
                    date.Mid(11, 2).ToLong(&hour) && date.Mid(14, 2).ToLong(&min) && date.Mid(17, 2).ToLong(&sec)) {
                    return GetMJD(year, month, day, hour, min, sec);
                }
            } else if (date.Len() == 15) {
                if (date.Mid(0, 2).ToLong(&day) && date.Mid(2, 2).ToLong(&month) && date.Mid(4, 4).ToLong(&year) &&
                    date.Mid(9, 2).ToLong(&hour) && date.Mid(11, 2).ToLong(&min) && date.Mid(13, 2).ToLong(&sec)) {
                    return GetMJD(year, month, day, hour, min, sec);
                }
            }
            break;

        case (YYYY_MM_DD_hh_mm_ss):

            if (date.Len() == 19) {
                if (date.Mid(0, 4).ToLong(&year) && date.Mid(5, 2).ToLong(&month) && date.Mid(8, 2).ToLong(&day) &&
                    date.Mid(11, 2).ToLong(&hour) && date.Mid(14, 2).ToLong(&min) && date.Mid(17, 2).ToLong(&sec)) {
                    return GetMJD(year, month, day, hour, min, sec);
                }
            } else if (date.Len() == 15) {
                if (date.Mid(0, 4).ToLong(&year) && date.Mid(4, 2).ToLong(&month) && date.Mid(6, 2).ToLong(&day) &&
                    date.Mid(9, 2).ToLong(&hour) && date.Mid(11, 2).ToLong(&min) && date.Mid(13, 2).ToLong(&sec)) {
                    return GetMJD(year, month, day, hour, min, sec);
                }
            }
            break;

        case (guess):

            if (date.Len() == 10) {
                if (date.Mid(0, 4).ToLong(&year)) {
                    if (date.Mid(0, 4).ToLong(&year) && date.Mid(5, 2).ToLong(&month) && date.Mid(8, 2).ToLong(&day)) {
                        return GetMJD(year, month, day);
                    }
                } else if (date.Mid(0, 2).ToLong(&day)) {
                    if (date.Mid(0, 2).ToLong(&day) && date.Mid(3, 2).ToLong(&month) && date.Mid(6, 4).ToLong(&year)) {
                        return GetMJD(year, month, day);
                    }
                }
            } else if (date.Len() == 16) {
                if (date.Mid(0, 4).ToLong(&year)) {
                    if (date.Mid(0, 4).ToLong(&year) && date.Mid(5, 2).ToLong(&month) && date.Mid(8, 2).ToLong(&day) &&
                        date.Mid(11, 2).ToLong(&hour) && date.Mid(14, 2).ToLong(&min)) {
                        return GetMJD(year, month, day, hour, min);
                    }
                } else if (date.Mid(0, 2).ToLong(&day)) {
                    if (date.Mid(0, 2).ToLong(&day) && date.Mid(3, 2).ToLong(&month) && date.Mid(6, 4).ToLong(&year) &&
                        date.Mid(11, 2).ToLong(&hour) && date.Mid(14, 2).ToLong(&min)) {
                        return GetMJD(year, month, day, hour, min);
                    }
                }
            } else if (date.Len() == 19) {
                if (date.Mid(0, 4).ToLong(&year)) {
                    if (date.Mid(0, 4).ToLong(&year) && date.Mid(5, 2).ToLong(&month) && date.Mid(8, 2).ToLong(&day) &&
                        date.Mid(11, 2).ToLong(&hour) && date.Mid(14, 2).ToLong(&min) && date.Mid(17, 2).ToLong(&sec)) {
                        return GetMJD(year, month, day, hour, min, sec);
                    }
                } else if (date.Mid(0, 2).ToLong(&day)) {
                    if (date.Mid(0, 2).ToLong(&day) && date.Mid(3, 2).ToLong(&month) && date.Mid(6, 4).ToLong(&year) &&
                        date.Mid(11, 2).ToLong(&hour) && date.Mid(14, 2).ToLong(&min) && date.Mid(17, 2).ToLong(&sec)) {
                        return GetMJD(year, month, day, hour, min, sec);
                    }
                }
            }
            break;
    }

    throw InvalidArgument(wxString::Format(_("The date (%s) conversion failed. Please check the format"), dateStr));
}

double GetMJD(int year, int month, int day, int hour, int minute, int second) {
    wxASSERT(year > 0);
    wxASSERT(month > 0);
    wxASSERT(day > 0);
    wxASSERT(hour >= 0);
    wxASSERT(minute >= 0);
    wxASSERT(second >= 0);

    double mjd = 0;

    if (year < 0) year++;
    double yearCorr = (year > 0 ? 0.0 : 0.75);
    if (month <= 2) {
        year--;
        month += 12;
    }
    int b = 0;
    if (year * 10000.0 + month * 100.0 + day >= 15821015.0) {
        int a = year / 100;
        b = 2 - a + a / 4;
    }
    mjd = (long)(365.25 * year - yearCorr) + (long)(30.6001 * (month + 1)) + day + 1720995L + b;

    // The hour part
    mjd += (double)hour / 24 + (double)minute / 1440 + (double)second / 86400;

    // Set to Modified Julian Day
    mjd -= 2400001;  // And not 2400000.5 (not clear why)

    return mjd;
}
