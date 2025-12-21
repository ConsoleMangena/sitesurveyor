# Contributing to SiteSurveyor

Thank you for your interest in contributing to SiteSurveyor! This document provides guidelines and information for contributors.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Project Structure](#project-structure)
- [Making Changes](#making-changes)
- [Coding Standards](#coding-standards)
- [Submitting Changes](#submitting-changes)
- [Issue Guidelines](#issue-guidelines)

## Code of Conduct

This project adheres to the [Contributor Covenant Code of Conduct](CODE_OF_CONDUCT.md). By participating, you are expected to uphold this code.

## Getting Started

### Prerequisites

- **CMake** 3.16 or higher
- **Qt6** (Core, Gui, Widgets, Concurrent, PrintSupport, Network)
- **C++17** compatible compiler
  - GCC 9+ (Linux)
  - Clang 10+ (macOS)
  - MSVC 2019+ (Windows)
- **GDAL** 3.x (optional, for GIS format support)
- **GEOS** 3.x (optional, for geometry operations)

### Fork and Clone

1. Fork the repository on GitHub
2. Clone your fork locally:
   ```bash
   git clone https://github.com/YOUR_USERNAME/sitesurveyor.git
   cd sitesurveyor
   git submodule update --init --recursive
   ```

3. Add the upstream remote:
   ```bash
   git remote add upstream https://github.com/ConsoleMangena/sitesurveyor.git
   ```

## Development Setup

### Linux (Ubuntu/Debian)

```bash
# Install dependencies
sudo apt install build-essential cmake ninja-build \
    qt6-base-dev libqt6concurrent6 libqt6network6 libqt6printsupport6 \
    libgdal-dev libgeos-dev

# Configure and build
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Debug
ninja

# Run
./bin/SiteSurveyor
```

### Windows

See [WINDOWS_BUILD.md](WINDOWS_BUILD.md) for detailed Windows setup instructions.

### macOS

```bash
# Install dependencies via Homebrew
brew install cmake qt@6 gdal geos

# Configure and build
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6) -DCMAKE_BUILD_TYPE=Debug
make -j$(sysctl -n hw.ncpu)

# Run
open SiteSurveyor.app
```

## Project Structure

```
sitesurveyor/
├── src/                    # Source files (.cpp)
│   ├── app/                # Application entry, main window, dialogs
│   ├── auth/               # Authentication and cloud sync
│   ├── canvas/             # Map canvas and rendering
│   ├── categories/         # Survey category definitions
│   ├── console/            # Command console
│   ├── gama/               # GNU Gama integration
│   ├── gdal/               # GDAL/OGR/GEOS interface
│   └── tools/              # Survey calculation dialogs
├── include/                # Header files (.h) - mirrors src structure
├── resources/              # Qt resources (icons, styles, etc.)
├── external/               # Third-party dependencies
│   ├── ads/                # Qt Advanced Docking System
│   └── lib/                # Bundled libraries
└── CMakeLists.txt          # Build configuration
```

### Key Components

| Component | Location | Description |
|-----------|----------|-------------|
| MainWindow | `src/app/mainwindow.cpp` | Central application hub |
| CanvasWidget | `src/canvas/canvaswidget.cpp` | 2D survey rendering |
| CategoryManager | `src/categories/` | Survey type definitions |
| Survey Tools | `src/tools/` | COGO calculation dialogs |
| GIS I/O | `src/gdal/` | File import/export |
| Network Adjustment | `src/gama/` | GNU Gama integration |

## Making Changes

### Branch Naming

Use descriptive branch names:

- `feature/description` - New features
- `fix/description` - Bug fixes
- `docs/description` - Documentation updates
- `refactor/description` - Code refactoring

### Development Workflow

1. Create a new branch from `main`:
   ```bash
   git checkout main
   git pull upstream main
   git checkout -b feature/your-feature-name
   ```

2. Make your changes with clear, atomic commits

3. Keep your branch up to date:
   ```bash
   git fetch upstream
   git rebase upstream/main
   ```

4. Push your branch:
   ```bash
   git push origin feature/your-feature-name
   ```

## Coding Standards

### C++ Style

- Use **C++17** features appropriately
- Follow Qt naming conventions:
  - Classes: `PascalCase` (e.g., `CanvasWidget`)
  - Functions/methods: `camelCase` (e.g., `updateLayerPanel`)
  - Member variables: `m_` prefix (e.g., `m_canvas`)
  - Constants: `UPPER_SNAKE_CASE` or `k` prefix
- Use modern C++ idioms (smart pointers, range-based for, etc.)

### Header Files

```cpp
#ifndef CLASSNAME_H
#define CLASSNAME_H

// Includes...

class ClassName : public QWidget
{
    Q_OBJECT

public:
    explicit ClassName(QWidget *parent = nullptr);
    ~ClassName();

signals:
    void someSignal();

public slots:
    void someSlot();

private:
    // Member variables with m_ prefix
    Type* m_member{nullptr};
};

#endif // CLASSNAME_H
```

### Comments

- Use `//` for single-line comments
- Use `/* */` for multi-line comments
- Document public APIs with Doxygen-style comments:
  ```cpp
  /**
   * @brief Brief description
   * @param param1 Description of param1
   * @return Description of return value
   */
  ```

### Qt Specifics

- Prefer Qt containers (`QVector`, `QMap`, `QString`) for Qt API interaction
- Use `nullptr` instead of `NULL` or `0`
- Connect signals/slots using the modern syntax:
  ```cpp
  connect(button, &QPushButton::clicked, this, &MyClass::onButtonClicked);
  ```

### Code Formatting

The project includes configuration files for consistent formatting:

| File | Purpose |
|------|---------|
| `.editorconfig` | Editor-agnostic formatting (indentation, line endings) |
| `.clang-format` | C++ code formatting rules |
| `.clang-tidy` | Static analysis and naming conventions |

**Format your code before committing:**

```bash
# Format all source files
find src include -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# Run static analysis
clang-tidy src/**/*.cpp -- -I include
```

## Submitting Changes

### Pull Request Process

1. Ensure your code compiles without warnings
2. Test your changes thoroughly
3. Update documentation if needed
4. Create a pull request with:
   - Clear title describing the change
   - Description of what and why
   - Reference to any related issues

### Pull Request Template

```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Documentation update
- [ ] Refactoring

## Testing
Describe testing performed

## Screenshots (if applicable)
Add screenshots for UI changes

## Related Issues
Fixes #123
```

## Issue Guidelines

### Bug Reports

When reporting bugs, please include:

- SiteSurveyor version
- Operating system and version
- Steps to reproduce
- Expected vs actual behavior
- Screenshots or error messages
- Sample files (if applicable)

### Feature Requests

For feature requests, describe:

- The problem or use case
- Proposed solution
- Alternatives considered
- Survey category relevance (Engineering, Cadastral, etc.)

## Questions?

Feel free to open an issue for questions or reach out through the official channels at [sitesurveyor.dev](https://sitesurveyor.dev).

---

Thank you for contributing to SiteSurveyor! 🎯
