#include "Utils.h"

#include <algorithm>
#include <wx/ffile.h>
#include <wx/fileconf.h>
#include <wx/stdpaths.h>

// New centralized date/time utilities
#include "UtilsDateTime.h"

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
    return value == INT_NAN_SENTINEL;
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
    wxASSERT(start);
    wxASSERT(end);
    wxASSERT(start <= end);

    // Convert from inclusive [start, end] to standard [start, end) range
    const T* searchEnd = end + 1;

    // Handle single-element or multi-element array
    if (start == end) {
        // Single element array - check if it matches
        if (NearlyEqual(value, *start, tolerance)) {
            return 0;
        }
        if (showWarning) {
            wxLogWarning(_("The value was not found in the array."));
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
                wxLogWarning(_("The value (%f) is out of the array range."), static_cast<float>(value));
            }
            return OUT_OF_RANGE;
        }
    } else {
        if (value > *start || value < *end) {
            if (showWarning) {
                wxLogWarning(_("The value (%f) is out of the array range."), static_cast<float>(value));
            }
            return OUT_OF_RANGE;
        }
    }

    if (showWarning) {
        wxLogWarning(_("The value was not found in the array."));
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
            wxLogError(_("The provided time step unit is not allowed."));
    }

    return 0;
}

Time GetTimeStructFromMJD(double mjd) {
    wxASSERT(mjd >= 0);

    return UtilsDateTime::FromMJD(mjd);
}

double ParseDate(const string& dateStr, TimeFormat format) {
    return UtilsDateTime::ParseToMJD(static_cast<const std::string&>(dateStr), format);
}

double GetMJD(int year, int month, int day, int hour, int minute, int second) {
    wxASSERT(year > 0);
    wxASSERT(month > 0);
    wxASSERT(day > 0);
    wxASSERT(hour >= 0);
    wxASSERT(minute >= 0);
    wxASSERT(second >= 0);

    return UtilsDateTime::ToMJD(year, month, day, hour, minute, second);
}
