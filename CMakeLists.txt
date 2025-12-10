cmake_minimum_required(VERSION 3.16)
project(SiteSurveyor VERSION 1.0.7 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt (support Qt6 and Qt5)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_standard_project_setup()
endif()

# Find GDAL
find_package(GDAL REQUIRED)
if(GDAL_FOUND)
    message(STATUS "GDAL found: ${GDAL_VERSION}")
    message(STATUS "GDAL include dirs: ${GDAL_INCLUDE_DIRS}")
    message(STATUS "GDAL libraries: ${GDAL_LIBRARIES}")
endif()

# Find GEOS C library (stable C API)
find_library(GEOS_C_LIBRARY NAMES geos_c
    HINTS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib
)
find_path(GEOS_INCLUDE_DIR geos_c.h
    HINTS /usr/include /usr/local/include
)

if(GEOS_C_LIBRARY AND GEOS_INCLUDE_DIR)
    message(STATUS "GEOS C library found: ${GEOS_C_LIBRARY}")
    message(STATUS "GEOS include dir: ${GEOS_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "GEOS C library not found!")
endif()

# Qt Advanced Docking System
set(QT_NO_PRIVATE_MODULE_WARNING ON)  # Suppress Qt private module warning

# ADS configuration
set(ADS_VERSION "4.3.1")
set(VERSION_SHORT "4.3.1")
set(VERSION_SONAME "4")
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/external/ads/cmake/modules ${CMAKE_MODULE_PATH})
set(BUILD_STATIC ON CACHE BOOL "Build static library" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Build examples" FORCE)
set(ADS_BUILD_EXAMPLES OFF CACHE BOOL "Build examples" FORCE)

add_subdirectory(external/ads EXCLUDE_FROM_ALL)

# Create alias for linking
add_library(qtadvanceddocking ALIAS qtadvanceddocking-qt${QT_VERSION_MAJOR})

# Source files
set(SOURCES
    src/app/main.cpp
    src/app/mainwindow.cpp
    src/app/settingsdialog.cpp
    src/canvas/canvaswidget.cpp
    src/gdal/gdalreader.cpp
    src/gdal/gdalgeosloader.cpp
    src/gdal/geosbridge.cpp
    src/tools/snapper.cpp
    src/tools/levellingdialog.cpp
    src/gama/gamaexporter.cpp
    src/gama/gamarunner.cpp
    resources/resources.qrc
)

# Header files
set(HEADERS
    include/app/mainwindow.h
    include/app/settingsdialog.h
    include/canvas/canvaswidget.h
    include/dxf/dxfreader.h
    include/gdal/gdalreader.h
    include/gdal/gdalgeosloader.h
    include/gdal/geosbridge.h
    include/tools/snapper.h
    include/tools/levellingdialog.h
    include/gama/gamaexporter.h
    include/gama/gamarunner.h
)

# Create executable
if(QT_VERSION_MAJOR EQUAL 6)
    qt_add_executable(SiteSurveyor MANUAL_FINALIZATION ${SOURCES} ${HEADERS})
else()
    add_executable(SiteSurveyor ${SOURCES} ${HEADERS})
endif()

# Includes and libraries
target_include_directories(SiteSurveyor PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${GDAL_INCLUDE_DIRS}
    ${GEOS_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/external/ads/src
)

target_link_libraries(SiteSurveyor PRIVATE 
    Qt${QT_VERSION_MAJOR}::Core 
    Qt${QT_VERSION_MAJOR}::Widgets
    ${GDAL_LIBRARIES}
    ${GEOS_C_LIBRARY}
    qtadvanceddocking
)

# Windows: embed application icon in the executable (optional if ICO present)
if(WIN32)
    set(APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows/sitesurveyor.ico")
    if(EXISTS "${APP_ICON}")
        target_sources(SiteSurveyor PRIVATE resources/windows/SiteSurveyor.rc)
    else()
        message(STATUS "Windows icon not found; building without embedded icon")
    endif()
endif()

# Set properties
set_target_properties(SiteSurveyor PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

# Place the executable in a predictable folder (non-macOS)
if(NOT APPLE)
    set_target_properties(SiteSurveyor PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()

# Install target and desktop integration
include(GNUInstallDirs)
install(TARGETS SiteSurveyor RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES resources/sitesurveyor.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
install(FILES resources/logo/icons8-land-240.svg DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps RENAME sitesurveyor.svg)
install(FILES resources/logo/sitesurveyor_256.png DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps RENAME sitesurveyor.png)
install(FILES resources/logo/sitesurveyor_128.png DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/128x128/apps RENAME sitesurveyor.png)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(SiteSurveyor)
endif()
