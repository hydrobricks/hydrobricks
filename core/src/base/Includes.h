#ifndef INCLUDES_H
#define INCLUDES_H

//---------------------------------
// Standard wxWidgets headers
//---------------------------------

// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>

#ifdef __BORLANDC__
#pragma hdrstop
#endif

// For all others, include the necessary headers
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

//---------------------------------
// Eigen library
//---------------------------------

#include <Eigen/Dense>

//---------------------------------
// wxWidgets library - frequently used classes
//---------------------------------

#ifndef WX_PRECOMP
#include <wx/arrstr.h>
#include <wx/fileconf.h>
#include <wx/log.h>
#include <wx/string.h>
#include <wx/utils.h>
#endif

#if defined(__WIN32__)
#include <wx/msw/regconf.h>  // wxRegConfig class
#endif

//---------------------------------
// Standard library
//---------------------------------

#include <algorithm>
#include <cmath>
#include <exception>
#include <numeric>
#include <vector>

//---------------------------------
// Automatic leak detection with Microsoft VisualC++
// http://msdn.microsoft.com/en-us/library/e5ewb1h3(v=VS.90).aspx
// http://wiki.wxwidgets.org/Avoiding_Memory_Leaks
//---------------------------------

#ifdef _DEBUG

#include <stdlib.h>
#include <wx/debug.h>  // wxASSERT

#ifdef __WXMSW__
#include <crtdbg.h>
#include <wx/msw/msvcrt.h>  // redefines the new() operator

#if !defined(_INC_CRTDBG) || !defined(_CRTDBG_MAP_ALLOC)
#pragma message("Debug CRT functions have not been included!")
#endif
#endif

#ifdef USE_VLD
#include <vld.h>  // Visual Leak Detector (https://vld.codeplex.com/)
#endif

#endif

//---------------------------------
// Typedefs
//---------------------------------

typedef std::vector<std::string> vecStr;
typedef std::vector<int> vecInt;
typedef std::vector<float> vecFloat;
typedef std::vector<double> vecDouble;
typedef std::vector<double*> vecDoublePt;
typedef Eigen::ArrayXd axd;
typedef Eigen::ArrayXi axi;
typedef Eigen::ArrayXXd axxd;
typedef std::vector<Eigen::ArrayXd> vecAxd;
typedef std::vector<Eigen::ArrayXXd> vecAxxd;

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
// Own exceptions
//---------------------------------

class NotImplemented : public std::logic_error {
  public:
    NotImplemented()
        : std::logic_error("Function not yet implemented"){};
};

class ShouldNotHappen : public std::logic_error {
  public:
    ShouldNotHappen()
        : std::logic_error("This should not happen..."){};
};

class InvalidArgument : public std::invalid_argument {
  public:
    explicit InvalidArgument(const wxString& msg)
        : std::invalid_argument(msg){};
};

class MissingParameter : public std::logic_error {
  public:
    explicit MissingParameter(const wxString& msg)
        : std::logic_error(msg){};
};

class ConceptionIssue : public std::logic_error {
  public:
    explicit ConceptionIssue(const wxString& msg)
        : std::logic_error(msg){};
};

class NotFound : public std::logic_error {
  public:
    explicit NotFound(const wxString& msg)
        : std::logic_error(msg){};
};

//---------------------------------
// Own classes
//---------------------------------

#include "Enums.h"
#include "GlobVars.h"
#include "TypeDefs.h"
#include "Utils.h"

#endif  // INCLUDES_H
