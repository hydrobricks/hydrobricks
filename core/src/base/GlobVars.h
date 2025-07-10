#ifndef GLOB_VARS_H
#define GLOB_VARS_H

// Config
namespace config {
extern double timeStepInDays;  // Default time step in days
}

// Constants
namespace constants {
constexpr double dayInSec = 86400.0;
constexpr double iceDensity = 900.0;     // kg/m3
constexpr double snowDensity = 250.0;    // kg/m3
constexpr double waterDensity = 1000.0;  // kg/m3
}  // namespace constants

#endif  // GLOB_VARS_H
