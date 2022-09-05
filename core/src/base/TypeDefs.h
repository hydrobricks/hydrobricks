#ifndef HYDROBRICKS_TYPEDEFS_H
#define HYDROBRICKS_TYPEDEFS_H

/* NaN */
static const short NAN_S = std::numeric_limits<short>::max();
static const int NAN_I = std::numeric_limits<int>::max();
static const float NAN_F = std::numeric_limits<float>::quiet_NaN();
static const double NAN_D = std::numeric_limits<double>::quiet_NaN();

static const double EPSILON_D = std::numeric_limits<double>::epsilon();
static const double EPSILON_F = std::numeric_limits<float>::epsilon();
static const double PRECISION = 0.00000001;

#endif  // HYDROBRICKS_TYPEDEFS_H
