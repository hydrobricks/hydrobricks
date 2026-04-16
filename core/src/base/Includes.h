#ifndef INCLUDES_H
#define INCLUDES_H

//---------------------------------
// Eigen library
//---------------------------------

#include <Eigen/Dense>

//---------------------------------
// Standard library
//---------------------------------

#include <algorithm>
#include <cassert>
#include <cmath>
#include <exception>
#include <filesystem>
#include <format>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

//---------------------------------
// Typedefs
//---------------------------------

using std::vector;
typedef std::string string;
typedef vector<string> vecStr;
typedef vector<int> vecInt;
typedef vector<float> vecFloat;
typedef vector<double> vecDouble;
typedef vector<double*> vecDoublePt;
typedef Eigen::ArrayXd axd;
typedef Eigen::ArrayXi axi;
typedef Eigen::ArrayXXd axxd;
typedef vector<Eigen::ArrayXd> vecAxd;
typedef vector<Eigen::ArrayXXd> vecAxxd;

// A time structure
typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int min;
    int sec;
} Time;

//---------------------------------
// Own classes
//---------------------------------

#include "Enums.h"
#include "Exceptions.h"
#include "GlobVars.h"
#include "Log.h"
#include "TypeDefs.h"
#include "Utils.h"

#endif  // INCLUDES_H
