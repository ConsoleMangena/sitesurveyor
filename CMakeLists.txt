cmake_minimum_required(VERSION 3.16)
project(SiteSurveyor VERSION 1.0.8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt (support Qt6 and Qt5)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Concurrent PrintSupport Network)
set(QT_VERSION_MAJOR 6)

if(QT_VERSION_MAJOR EQUAL 6)
    # qt_standard_project_setup() was added in Qt 6.3
    if(COMMAND qt_standard_project_setup)
        qt_standard_project_setup()
    endif()
endif()

# Find GDAL
option(WITH_GDAL "Build with GDAL support" ON)

if(WITH_GDAL)
    if(WIN32)
        find_package(GDAL CONFIG)
    endif()

    if(NOT GDAL_FOUND)
        find_package(GDAL REQUIRED)
    endif()
    
    if(GDAL_FOUND)
        message(STATUS "GDAL found: ${GDAL_VERSION}")
        message(STATUS "GDAL include dirs: ${GDAL_INCLUDE_DIRS}")
        message(STATUS "GDAL libraries: ${GDAL_LIBRARIES}")
        add_compile_definitions(HAVE_GDAL)
    endif()
else()
    message(STATUS "GDAL disabled by WITH_GDAL=OFF")
endif()


# Find GEOS
option(WITH_GEOS "Build with GEOS support" ON)

if(WITH_GEOS)
    if(WIN32)
        find_package(geos CONFIG)
    endif()
    
    if(geos_FOUND)
        message(STATUS "GEOS found via CONFIG: ${geos_VERSION}")
        set(HAVE_GEOS TRUE)
        add_compile_definitions(HAVE_GEOS)
        # vcpkg usually defines GEOS::geos_c
        if(TARGET GEOS::geos_c)
            set(GEOS_C_LIBRARY GEOS::geos_c)
        elseif(TARGET geos::geos_c)
            set(GEOS_C_LIBRARY geos::geos_c)
        endif()
    else()
        # Fallback to manual search if WITH_GEOS is on but config not found
        find_library(GEOS_C_LIBRARY NAMES geos_c
            HINTS 
                /usr/lib 
                /usr/lib/x86_64-linux-gnu 
                /usr/local/lib
                $ENV{GEOS_DIR}/lib
                $ENV{GEOS_DIR}/bin
                "C:/OSGeo4W/lib"
                "C:/OSGeo4W64/lib"
        )
        
        find_path(GEOS_INCLUDE_DIR NAMES geos_c.h
            HINTS
                /usr/include
                /usr/local/include
                $ENV{GEOS_DIR}/include
                "C:/OSGeo4W/include"
                "C:/OSGeo4W64/include"
        )
        
        if(GEOS_C_LIBRARY AND GEOS_INCLUDE_DIR)
            set(HAVE_GEOS TRUE)
            add_compile_definitions(HAVE_GEOS)
            message(STATUS "GEOS found: ${GEOS_C_LIBRARY}")
        endif()
    endif()
    
    if(NOT HAVE_GEOS)
         message(FATAL_ERROR "GEOS not found but WITH_GEOS is ON.")
    endif()
else()
    message(STATUS "GEOS disabled by WITH_GEOS=OFF")
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
    src/app/startdialog.cpp
    src/app/settingsdialog.cpp
    src/canvas/canvaswidget.cpp
    src/gdal/gdalreader.cpp
    src/gdal/gdalwriter.cpp
    src/gdal/gdalgeosloader.cpp
    src/gdal/geosbridge.cpp
    src/tools/snapper.cpp
    src/tools/levellingdialog.cpp
    src/tools/resection_dialog.cpp
    src/tools/check_geometry_dialog.cpp
    src/tools/station_setup_dialog.cpp
    src/tools/check_point_dialog.cpp
    src/tools/intersection_dialog.cpp
    src/tools/polar_dialog.cpp
    src/tools/join_dialog.cpp
    src/auth/authmanager.cpp
    src/app/logindialog.cpp
    src/tools/traverse_dialog.cpp
    src/tools/volume_dialog.cpp
    src/tools/tin3dviewer.cpp
    src/tools/contour_dialog.cpp

    src/gama/gamaexporter.cpp
    src/gama/gamarunner.cpp
    src/gama/network_adjustment_dialog.cpp
    src/categories/category_manager.cpp
    src/categories/engineering/engineering_features.cpp
    src/categories/cadastral/cadastral_features.cpp
    src/categories/mining/mining_features.cpp
    src/categories/topographic/topographic_features.cpp
    src/categories/geodetic/geodetic_features.cpp
    src/console/command_console.cpp
    resources/resources.qrc
)

# Header files
set(HEADERS
    include/app/mainwindow.h
    include/app/startdialog.h
    include/app/settingsdialog.h
    include/canvas/canvaswidget.h
    include/dxf/dxfreader.h
    include/gdal/gdalreader.h
    include/gdal/gdalwriter.h
    include/gdal/gdalgeosloader.h
    include/gdal/geosbridge.h
    include/tools/snapper.h
    include/tools/levellingdialog.h
    include/tools/resection_dialog.h
    include/tools/check_geometry_dialog.h
    include/tools/station_setup_dialog.h
    include/tools/check_point_dialog.h
    include/tools/intersection_dialog.h
    include/tools/polar_dialog.h
    include/tools/join_dialog.h
    include/auth/authmanager.h
    include/app/logindialog.h
    include/tools/traverse_dialog.h
    include/tools/volume_dialog.h
    include/tools/tin3dviewer.h
    include/tools/contour_dialog.h

    include/gama/gamaexporter.h
    include/gama/gamarunner.h
    include/gama/network_adjustment_dialog.h
    include/categories/category_manager.h
    include/categories/engineering/engineering_features.h
    include/categories/cadastral/cadastral_features.h
    include/categories/mining/mining_features.h
    include/categories/topographic/topographic_features.h
    include/categories/geodetic/geodetic_features.h
    include/console/command_console.h
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
    ${CMAKE_CURRENT_SOURCE_DIR}/external/ads/src
)

if(HAVE_GEOS)
    target_include_directories(SiteSurveyor PRIVATE ${GEOS_INCLUDE_DIR})
    target_compile_definitions(SiteSurveyor PRIVATE HAVE_GEOS=1)
endif()

target_link_libraries(SiteSurveyor PRIVATE 
    Qt${QT_VERSION_MAJOR}::Core 
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::PrintSupport
    ${GDAL_LIBRARIES}
    qtadvanceddocking
)



if(HAVE_GEOS)
    target_link_libraries(SiteSurveyor PRIVATE ${GEOS_C_LIBRARY})
endif()

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

# macOS: App Icon
if(APPLE)
    # Define icon source
    set(MACOSX_ICON_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/resources/logo/SiteSurveyor.icns")
    
    # If .icns doesn't exist, we can't easily generate it here without tools (iconutil).
    # But we can reference it so it works when the file is added.
    set_target_properties(SiteSurveyor PROPERTIES
        MACOSX_BUNDLE_ICON_FILE "SiteSurveyor.icns"
        MACOSX_BUNDLE_BUNDLE_NAME "SiteSurveyor"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.consolemangena.sitesurveyor"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )

    if(EXISTS "${MACOSX_ICON_SOURCE}")
        target_sources(SiteSurveyor PRIVATE "${MACOSX_ICON_SOURCE}")
        set_source_files_properties("${MACOSX_ICON_SOURCE}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    endif()
endif()

# Place the executable in a predictable folder (non-macOS)
if(NOT APPLE)
    set_target_properties(SiteSurveyor PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()

# Install target and desktop integration
include(GNUInstallDirs)
install(TARGETS SiteSurveyor 
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)
install(FILES resources/sitesurveyor.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
install(FILES "resources/logo/SiteSurveyor Vector.svg" DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps RENAME sitesurveyor.svg)
# install(FILES resources/logo/sitesurveyor_256.png DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps RENAME sitesurveyor.png)
# install(FILES resources/logo/sitesurveyor_128.png DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/128x128/apps RENAME sitesurveyor.png)

# Bundle GNU GAMA (gama-local) with the application
if(EXISTS "${CMAKE_SOURCE_DIR}/external/bin/gama-local")
    # Copy to build directory
    add_custom_command(TARGET SiteSurveyor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/external/bin/gama-local"
        "$<TARGET_FILE_DIR:SiteSurveyor>/gama-local"
        COMMENT "Bundling gama-local with application"
    )
    # Install alongside the application
    install(PROGRAMS external/bin/gama-local DESTINATION ${CMAKE_INSTALL_BINDIR})
    message(STATUS "GNU GAMA (gama-local) will be bundled with the application")
else()
    message(WARNING "GNU GAMA not found at external/bin/gama-local - users will need to install it separately")
endif()

# Bundle GIS libraries (GDAL, GEOS, PROJ) with the application
set(BUNDLE_GIS_LIBS TRUE CACHE BOOL "Bundle GDAL, GEOS, PROJ libraries with the application")

if(BUNDLE_GIS_LIBS AND EXISTS "${CMAKE_SOURCE_DIR}/external/lib")
    # List of bundled libraries
    set(BUNDLED_LIBS
        "${CMAKE_SOURCE_DIR}/external/lib/libgdal.so.38"
        "${CMAKE_SOURCE_DIR}/external/lib/libgeos_c.so.1"
        "${CMAKE_SOURCE_DIR}/external/lib/libgeos.so.3.14.1"
        "${CMAKE_SOURCE_DIR}/external/lib/libproj.so.25"
    )
    
    # Copy libraries to build directory
    foreach(LIB ${BUNDLED_LIBS})
        if(EXISTS "${LIB}")
            get_filename_component(LIB_NAME "${LIB}" NAME)
            add_custom_command(TARGET SiteSurveyor POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${LIB}"
                "$<TARGET_FILE_DIR:SiteSurveyor>/../lib/${LIB_NAME}"
                COMMENT "Bundling ${LIB_NAME}"
            )
        endif()
    endforeach()
    
    # Create lib directory at build time
    add_custom_command(TARGET SiteSurveyor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:SiteSurveyor>/../lib"
        COMMENT "Creating lib directory"
    )
    
    # Install bundled libraries
    install(FILES ${BUNDLED_LIBS} DESTINATION ${CMAKE_INSTALL_LIBDIR})
    
    # Set RPATH to find bundled libraries
    set_target_properties(SiteSurveyor PROPERTIES
        INSTALL_RPATH "$ORIGIN/../lib"
        BUILD_RPATH "$ORIGIN/../lib"
    )
    
    message(STATUS "GIS libraries (GDAL, GEOS, PROJ) will be bundled with the application")
else()
    message(STATUS "Using system GIS libraries (GDAL, GEOS, PROJ)")
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(SiteSurveyor)
endif()
