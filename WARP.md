# WARP.md

This file provides guidance to WARP (warp.dev) when working with code in this repository.

## Project Overview

SiteSurveyor is a Qt6-based desktop surveying and geomatics application written in C++17. It provides tools for surveying calculations, point management, line drawing, and coordinate system operations with a CAD-like interface.

## Development Commands

### Build System
```bash
# Create build directory and configure
mkdir build && cd build
cmake ..

# Build the application
cmake --build .

# Run the application
./bin/SiteSurveyor
```

### Alternative build approach
```bash
# Build from project root
cmake -B build -S .
cmake --build build
./build/bin/SiteSurveyor
```

### Testing Individual Components
- No formal test suite is present - test by running the application
- Test specific functionality through the command interface or GUI interactions

## Architecture Overview

### Core Application Structure

**Main Application Flow:**
- `main.cpp` initializes Qt application with dark theme and launches `MainWindow`
- `MainWindow` serves as the central coordinator, managing all major subsystems
- All components communicate through Qt signals/slots mechanism

**Key Architectural Components:**

**Point Management System:**
- `PointManager`: Central repository for survey points with add/remove/query operations
- `Point`: Simple data structure (name, x, y, z coordinates)
- Integrates with canvas for visual representation and table display

**Canvas and Visualization:**
- `CanvasWidget`: Custom Qt widget handling all 2D drawing operations
- Supports multiple tool modes: Select, Pan, ZoomWindow, DrawLine, DrawPolygon
- Implements coordinate transformations between screen and world coordinates
- Handles grid display, point/line rendering, and user interaction

**Command Processing:**
- `CommandProcessor`: Text-based command interpreter for surveying operations
- Processes commands like point addition, distance calculation, polar coordinates
- Integrates with `SurveyCalculator` for mathematical operations

**Survey Mathematics:**
- `SurveyCalculator`: Static utility class for surveying calculations
- Handles coordinate conversions (rectangular ↔ polar)
- Distance, azimuth, and area calculations
- Angle normalization and DMS formatting

**Layer Management:**
- `LayerManager`: Organizes drawing elements into layers
- `LayerPanel`: UI for layer visibility and management
- Each point and line can be assigned to specific layers

**User Interface Architecture:**
- **Docked Panels:** Points table, command interface, layers, and properties
- **Toolbar:** Tool selection and common operations
- **Status Bar:** Real-time coordinate display and mode indicators
- **Dialogs:** Settings, polar input, and join operations

### Data Flow Patterns

**Point Creation:**
1. User input → `CommandProcessor` or direct UI interaction
2. `PointManager` validates and stores point
3. Canvas receives notification and updates display
4. Points table refreshes automatically

**Canvas Interaction:**
1. Mouse events processed by `CanvasWidget`
2. Coordinate transformations applied (screen ↔ world)
3. Tool-specific behavior executed (select, pan, draw, etc.)
4. Visual feedback and signal emission to main window

**Layer System:**
1. `LayerManager` maintains layer definitions and visibility
2. Canvas queries layer settings during rendering
3. UI panels reflect current layer state
4. All drawing elements associated with specific layers

### File Organization

**Headers:** All in `include/` directory
**Source:** All in `src/` directory  
**Resources:** Icons and Qt resource files in `resources/`
**Build:** CMake-based build system with Qt6 integration

### Qt Integration Patterns

- **MOC (Meta-Object Compiler):** Auto-enabled for signal/slot mechanism
- **Resource System:** Icons embedded via Qt resource files (.qrc)
- **Dock Widgets:** Flexible UI layout with dockable panels
- **Undo/Redo:** QUndoStack integration for command history
- **Settings:** QSettings integration for application preferences

## Development Notes

### Key Dependencies
- Qt6 Core and Widgets modules
- CMake 3.16+ required
- C++17 standard

### Coordinate Systems
- Application uses standard Cartesian coordinates
- Canvas transforms between screen pixels and world coordinates
- Gauss mode available for specialized coordinate display

### Command Interface
Text commands processed through `CommandProcessor`:
- Point operations: `addpoint`, `deletepoint`, `listpoints`
- Calculations: `distance`, `azimuth`, `area`  
- Drawing: `line`, `polar`
- Utility: `clear`, `help`