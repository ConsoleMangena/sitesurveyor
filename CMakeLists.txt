cmake_minimum_required(VERSION 3.16)
project(SiteSurveyor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

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
    resources/resources.qrc
    src/layermanager.cpp
    src/layerpanel.cpp
    src/propertiespanel.cpp
    src/licensedialog.cpp
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
    include/layermanager.h
    include/layerpanel.h
    include/propertiespanel.h
    include/licensedialog.h
    include/welcomewidget.h
)

# Create executable
qt6_add_executable(SiteSurveyor ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(SiteSurveyor PRIVATE Qt6::Core Qt6::Widgets)

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

# Place the executable in a predictable folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Install target and desktop integration
install(TARGETS SiteSurveyor RUNTIME DESTINATION bin)
install(FILES resources/sitesurveyor.desktop DESTINATION share/applications)
install(FILES resources/icons/compass.svg DESTINATION share/icons/hicolor/scalable/apps RENAME sitesurveyor.svg)
