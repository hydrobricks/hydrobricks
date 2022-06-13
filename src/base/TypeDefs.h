#ifndef HYDROBRICKS_TYPEDEFS_H
#define HYDROBRICKS_TYPEDEFS_H

/* NaN */
static const short NaNs = std::numeric_limits<short>::max();
static const int NaNi = std::numeric_limits<int>::max();
static const float NaNf = std::numeric_limits<float>::quiet_NaN();
static const double NaNd = std::numeric_limits<double>::quiet_NaN();

static const double EPSILON_D = std::numeric_limits<double>::epsilon();
static const double EPSILON_F = std::numeric_limits<float>::epsilon();

#endif  // HYDROBRICKS_TYPEDEFS_H
