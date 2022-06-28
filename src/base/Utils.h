#ifndef HYDROBRICKS_UTILS_H
#define HYDROBRICKS_UTILS_H

#include "Includes.h"

bool IsNaN(int value);

bool IsNaN(float value);

bool IsNaN(double value);

int Find(const int *start, const int *end, int value, int tolerance = 0, bool showWarning = true);

int Find(const float *start, const float *end, float value, float tolerance = 0.0, bool showWarning = true);

int Find(const double *start, const double *end, double value, double tolerance = 0.0, bool showWarning = true);

template <class T>
int FindT(const T *start, const T *end, T value, T tolerance = 0, bool showWarning = true);

double IncrementDateBy(double date, int amount, TimeUnit unit);

void CheckNcStatus(int status);

Time GetTimeStructFromMJD(double mjd);

double ParseDate(const wxString &dateStr, TimeFormat format);

double GetMJD(int year, int month = 1, int day = 1, int hour = 0, int minute = 0, int second = 0);

#endif  // HYDROBRICKS_UTILS_H
