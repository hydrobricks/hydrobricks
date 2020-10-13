
#ifndef FLHY_UTILITIES_H
#define FLHY_UTILITIES_H

#include "Includes.h"

bool IsNaN(int value);

bool IsNaN(float value);

bool IsNaN(double value);

int Find(const int *start, const int *end, int value, int tolerance = 0, bool showWarning = true);

int Find(const float *start, const float *end, float value, float tolerance = 0.0, bool showWarning = true);

int Find(const double *start, const double *end, double value, double tolerance = 0.0, bool showWarning = true);

template <class T>
int FindT(const T *start, const T *end, T value, T tolerance = 0, bool showWarning = true);

#endif  // FLHY_UTILITIES_H
