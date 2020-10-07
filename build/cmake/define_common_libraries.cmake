
# WxWidgets
mark_as_advanced(wxWidgets_wxrc_EXECUTABLE)
mark_as_advanced(wxWidgets_with_GUI)
if (USE_MSYS2)
    set(wxWidgets_CONFIG_OPTIONS --prefix=${MINGW_PATH})
endif (USE_MSYS2)
set(wxWidgets_with_GUI FALSE)
find_package(wxWidgets REQUIRED base xml net)
include("${wxWidgets_USE_FILE}")
include_directories(${wxWidgets_INCLUDE_DIRS})
link_libraries(${wxWidgets_LIBRARIES})

# NetCDF
mark_as_advanced(CLEAR NETCDF_INCLUDE_DIR)
mark_as_advanced(CLEAR NETCDF_LIBRARY)
find_package(NetCDF 4 MODULE REQUIRED)
include_directories(${NETCDF_INCLUDE_DIRS})
link_libraries(${NETCDF_LIBRARIES})
