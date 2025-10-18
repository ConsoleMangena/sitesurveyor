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
    src/surveycalculator.cpp
    src/canvaswidget.cpp
    src/pointmanager.cpp
    src/commandprocessor.cpp
    src/settingsdialog.cpp
    src/appsettings.cpp
    src/joinpolardialog.cpp
    src/polarinputdialog.cpp
    resources/resources.qrc
    src/layermanager.cpp
    src/layerpanel.cpp
    src/propertiespanel.cpp
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
    include/layermanager.h
    include/layerpanel.h
    include/propertiespanel.h
)

# Create executable
qt6_add_executable(SiteSurveyor ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(SiteSurveyor PRIVATE Qt6::Core Qt6::Widgets)

# Set properties
set_target_properties(SiteSurveyor PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

# Place the executable in a predictable folder
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)