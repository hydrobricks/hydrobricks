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
#error Debug CRT functions have not been included!
#endif
#endif

#ifdef USE_VLD
#include <vld.h>  // Visual Leak Detector (https://vld.codeplex.com/)
#endif

#endif

//---------------------------------
// Own classes
//---------------------------------

#include "Enums.h"

#endif  // INCLUDES_H
