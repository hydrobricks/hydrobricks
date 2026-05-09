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
using string = std::string;
using vecStr = vector<string>;
using vecInt = vector<int>;
using vecFloat = vector<float>;
using vecDouble = vector<double>;
using vecDoublePt = vector<double*>;
using axd = Eigen::ArrayXd;
using axi = Eigen::ArrayXi;
using axxd = Eigen::ArrayXXd;
using vecAxd = vector<Eigen::ArrayXd>;
using vecAxxd = vector<Eigen::ArrayXXd>;

struct Time {
    int year;
    int month;
    int day;
    int hour;
    int min;
    int sec;
};

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
