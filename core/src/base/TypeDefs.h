#ifndef HYDROBRICKS_TYPEDEFS_H
#define HYDROBRICKS_TYPEDEFS_H

#include "NumericConstants.h"

/* NaN */
static const short NAN_S = std::numeric_limits<short>::max();
// Sentinel value used to represent "no value" for int type (used in IsNaN(int))
static const int INT_NAN_SENTINEL = std::numeric_limits<int>::max();
static const float NAN_F = std::numeric_limits<float>::quiet_NaN();
static const double NAN_D = std::numeric_limits<double>::quiet_NaN();

// Keep NAN_I for backward compatibility
static const int NAN_I = INT_NAN_SENTINEL;


#endif  // HYDROBRICKS_TYPEDEFS_H
