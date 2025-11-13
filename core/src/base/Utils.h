#ifndef HYDROBRICKS_UTILS_H
#define HYDROBRICKS_UTILS_H

#include "Includes.h"

/**
 * Initialization of the user directory and the config file.
 */
[[nodiscard]] bool InitHydrobricks();

/**
 * Initialization of hydrobricks for Python.
 */
[[nodiscard]] bool InitHydrobricksForPython();

/**
 * Get the user directory path.
 *
 * @return The user directory path.
 */
wxString GetUserDirPath();

/**
 * Check that the output directory exists and create it if not.
 *
 * @return True on success.
 */
[[nodiscard]] bool CheckOutputDirectory(const string& path);

/**
 * Display the processing time.
 *
 * @param sw The stop watch instance created before the computations.
 */
void DisplayProcessingTime(const wxStopWatch& sw);

/**
 * Create a log file at the given path.
 *
 * @param path Destination of the log file.
 */
void InitLog(const string& path);

/**
 * Close the log file.
 */
void CloseLog();

/**
 * Set the log level to max (max verbosity)
 */
void SetMaxLogLevel();

/**
 * Set the log level to debug
 */
void SetDebugLogLevel();

/**
 * Set the log level to message (standard)
 */
void SetMessageLogLevel();

/**
 * Check if the given integer value is a NaN.
 *
 * @param value The value to check.
 * @return True if the value is a NaN, false otherwise.
 */
bool IsNaN(int value);

/**
 * Check if the given float value is a NaN.
 *
 * @param value The value to check.
 * @return True if the value is a NaN, false otherwise.
 */
bool IsNaN(float value);

/**
 * Check if the given double value is a NaN.
 *
 * @param value The value to check.
 * @return True if the value is a NaN, false otherwise.
 */
bool IsNaN(double value);

/**
 * Ge the path separator of the OS used.
 *
 * @return The path separator of the current OS.
 */
const char* GetPathSeparator();

/**
 * Compare two strings in an case insensitive way.
 *
 * @param str1 First string
 * @param str2 Second string
 * @return True if strings match.
 * @note From https://thispointer.com/c-case-insensitive-string-comparison-using-stl-c11-boost-library/
 */
bool StringsMatch(const string& str1, const string& str2);

/**
 * Find an value in a vector of integers.
 *
 * @param start Pointer to the start of the search (e.g. start of the vector).
 * @param end Pointer to the end of the search (e.g. end of the vector).
 * @param value The value to search.
 * @param tolerance A tolerance for the searched value.
 * @param showWarning option to show a warning if the value was not found in the vector (default: true).
 * @return The index of the searched value.
 */
int Find(const int* start, const int* end, int value, int tolerance = 0, bool showWarning = true);

/**
 * Find an value in a vector of floats.
 *
 * @param start Pointer to the start of the search (e.g. start of the vector).
 * @param end Pointer to the end of the search (e.g. end of the vector).
 * @param value The value to search.
 * @param tolerance A tolerance for the searched value.
 * @param showWarning option to show a warning if the value was not found in the vector (default: true).
 * @return The index of the searched value.
 */
int Find(const float* start, const float* end, float value, float tolerance = 0.0, bool showWarning = true);

/**
 * Find an value in a vector of doubles.
 *
 * @param start Pointer to the start of the search (e.g. start of the vector).
 * @param end Pointer to the end of the search (e.g. end of the vector).
 * @param value The value to search.
 * @param tolerance A tolerance for the searched value.
 * @param showWarning option to show a warning if the value was not found in the vector (default: true).
 * @return The index of the searched value.
 */
int Find(const double* start, const double* end, double value, double tolerance = 0.0, bool showWarning = true);

/**
 * Find an value in a vector (template).
 *
 * @param start Pointer to the start of the search (e.g. start of the vector).
 * @param end Pointer to the end of the search (e.g. end of the vector).
 * @param value The value to search.
 * @param tolerance A tolerance for the searched value.
 * @param showWarning option to show a warning if the value was not found in the vector (default: true).
 * @return The index of the searched value.
 */
template <class T>
int FindT(const T* start, const T* end, T value, T tolerance = 0, bool showWarning = true);

/**
 * Increment the provided date by the given amount of time.
 *
 * @param date The date to increment provided as an MJD value.
 * @param amount The number of increment to apply.
 * @param unit The unit of the increment to apply.
 * @return The new date (as MJD) after incrementation.
 */
double IncrementDateBy(double date, int amount, TimeUnit unit);

/**
 * Create a time structure from a date provided as MJD.
 *
 * @param mjd The date provided as MJD.
 * @return The corresponding time structure.
 */
Time GetTimeStructFromMJD(double mjd);

/**
 * Parse a date provided as a string.
 *
 * @param dateStr The string containing the date to parse.
 * @param format The format of the date to parse.
 * @return The date value as MJD.
 */
double ParseDate(const string& dateStr, TimeFormat format);

/**
 * Get a date as an MJD value.
 *
 * @param year The year.
 * @param month The month.
 * @param day The day (default: 0).
 * @param hour The hour (default: 0).
 * @param minute The minute (default: 0).
 * @param second The second (default: 0).
 * @return The corresponding date as an MJD value.
 */
double GetMJD(int year, int month = 1, int day = 1, int hour = 0, int minute = 0, int second = 0);

#endif  // HYDROBRICKS_UTILS_H
