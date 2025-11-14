cmake_minimum_required(VERSION 3.16)
project(SiteSurveyor VERSION 1.0.0 LANGUAGES CXX)

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

# Source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/iconmanager.cpp
    src/surveycalculator.cpp
    src/canvaswidget.cpp
    src/pointmanager.cpp
    src/commandprocessor.cpp
    src/settingsdialog.cpp
    src/appsettings.cpp
    src/joinpolardialog.cpp
    src/polarinputdialog.cpp
    src/traversedialog.cpp
    src/projectplanpanel.cpp
    src/intersectresectiondialog.cpp
    src/levelingdialog.cpp
    src/lsnetworkdialog.cpp
    src/transformdialog.cpp
    src/masspolardialog.cpp
    src/stakeoutmanager.cpp
    src/controlregisterdialog.cpp
    src/stakeoutdialog.cpp
    src/coordinatesystemdialog.cpp
    src/stakeoutreportgenerator.cpp
    resources/resources.qrc
    src/layermanager.cpp
    src/layerpanel.cpp
    src/propertiespanel.cpp
    src/welcomewidget.cpp
)

# Header files
set(HEADERS
    include/mainwindow.h
    include/surveycalculator.h
    include/canvaswidget.h
    include/pointmanager.h
    include/commandprocessor.h
    include/point.h
    include/settingsdialog.h
    include/appsettings.h
    include/joinpolardialog.h
    include/polarinputdialog.h
    include/traversedialog.h
    include/projectplanpanel.h
    include/intersectresectiondialog.h
    include/levelingdialog.h
    include/lsnetworkdialog.h
    include/transformdialog.h
    include/masspolardialog.h
    include/stakeoutmanager.h
    include/controlregisterdialog.h
    include/stakeoutdialog.h
    include/coordinatesystemdialog.h
    include/stakeoutrecord.h
    include/stakeoutreportgenerator.h
    include/layermanager.h
    include/layerpanel.h
    include/propertiespanel.h
    include/welcomewidget.h
)

# Create executable
if(QT_VERSION_MAJOR EQUAL 6)
    qt_add_executable(SiteSurveyor MANUAL_FINALIZATION ${SOURCES} ${HEADERS})
else()
    add_executable(SiteSurveyor ${SOURCES} ${HEADERS})
endif()

# Includes and libraries
target_include_directories(SiteSurveyor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(SiteSurveyor PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Widgets)

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
# Install official logo into icon theme hierarchy
install(FILES resources/logo/icons8-land-240.svg DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps RENAME sitesurveyor.svg)
install(FILES resources/logo/sitesurveyor_256.png DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps RENAME sitesurveyor.png)
install(FILES resources/logo/sitesurveyor_128.png DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/128x128/apps RENAME sitesurveyor.png)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(SiteSurveyor)
endif()
