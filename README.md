# SiteSurveyor

<p align="center">
  <img src="resources/logo/SiteSurveyor Vector.svg" alt="SiteSurveyor Logo" width="120">
</p>

<p align="center">
  <strong>Professional Free Geomatics Software for Windows, Linux, and macOS</strong>
</p>

<p align="center">
  <a href="https://github.com/ConsoleMangena/sitesurveyor/releases"><img src="https://img.shields.io/github/v/release/ConsoleMangena/sitesurveyor?style=flat-square" alt="Release"></a>
  <a href="LICENSE"><img src="https://img.shields.io/github/license/ConsoleMangena/sitesurveyor?style=flat-square" alt="License"></a>
  <a href="https://github.com/ConsoleMangena/sitesurveyor/actions/workflows/ci.yml"><img src="https://img.shields.io/github/actions/workflow/status/ConsoleMangena/sitesurveyor/ci.yml?style=flat-square&label=CI" alt="CI"></a>
  <a href="https://github.com/ConsoleMangena/sitesurveyor/actions/workflows/release.yml"><img src="https://img.shields.io/github/actions/workflow/status/ConsoleMangena/sitesurveyor/release.yml?style=flat-square&label=Release" alt="Release Build"></a>
  <img src="https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-blue?style=flat-square" alt="Platform">
  <img src="https://img.shields.io/badge/Qt-6.6+-41CD52?style=flat-square&logo=qt" alt="Qt">
</p>


<p align="center">
  <a href="https://sitesurveyor.dev">Official Website</a> •
  <a href="https://github.com/ConsoleMangena/sitesurveyor/releases">Downloads</a> •
  <a href="#features">Features</a> •
  <a href="#installation">Installation</a> •
  <a href="#building-from-source">Build</a> •
  <a href="docs/USER_GUIDE.md">User Guide</a> •
  <a href="CONTRIBUTING.md">Contributing</a>
</p>




---

## Overview

SiteSurveyor is a cross-platform geomatics application designed for land surveyors, engineers, and GIS professionals. Built with Qt6 and C++17, it provides a complete toolkit for survey calculations, data visualization, and project management.

## Features

### 📐 Survey Categories

SiteSurveyor supports five specialized survey categories, each with tailored tools and default layers:

| Category | Specialized Features |
|----------|---------------------|
| **Engineering** | Setting out, as-built surveys, levelling, offset/partition |
| **Cadastral** | Boundary surveys, subdivisions, area calculations |
| **Mining** | Volume calculations, cross-sections, development tracking |
| **Topographic** | Contour generation, feature mapping, DTM |
| **Geodetic** | Control networks, GNSS integration, baseline processing |

### 🧮 COGO Calculations

- **Traverse Calculations** - Forward/backward traverse with Bowditch and Transit adjustments
- **Polar Calculations** - Bearing and distance computations
- **Join Calculations** - Coordinate inverse calculations
- **Intersection** - Bearing-bearing and bearing-distance intersections
- **Resection** - Free station setup from known points

### 📊 Network Adjustment

Integrates **GNU Gama** for rigorous least-squares network adjustment:
- Horizontal and 3D network adjustment
- Chi-square statistical testing
- Residual analysis and quality metrics
- Export to standard formats

### 🗺️ Data Import/Export

| Format | Import | Export |
|--------|:------:|:------:|
| DXF (AutoCAD) | ✅ | ✅ |
| Shapefile | ✅ | ✅ |
| GeoJSON | ✅ | ✅ |
| CSV/TXT Points | ✅ | ✅ |
| GeoTIFF (Raster) | ✅ | - |
| KML/KMZ | ✅ | ✅ |

### ☁️ Cloud Features

- **Project Sync** - Save and load projects from the cloud
- **Version History** - Track project changes over time
- **Project Sharing** - Collaborate with team members
- **Conflict Detection** - Automatic detection of concurrent edits

### 🎨 User Interface

- Modern, professional dark and light themes
- Dockable panels for layers, properties, and coordinates
- Interactive canvas with 24+ tool modes
- Animated zoom and pan with smooth transitions
- Customizable toolbar and keyboard shortcuts

## Installation

### Download Pre-built Binaries

Get the latest version from:
- **[Official Website](https://sitesurveyor.dev)** (recommended)
- **[GitHub Releases](https://github.com/ConsoleMangena/sitesurveyor/releases)**

### Linux

```bash
# Make executable and run
chmod +x SiteSurveyor-*.AppImage
./SiteSurveyor-*.AppImage
```

### Windows

Download the `.zip` file, extract to any folder, and run `SiteSurveyor.exe`.

## Building from Source

### Prerequisites

- **CMake** 3.16+
- **Qt6** (Core, Gui, Widgets, Concurrent, PrintSupport, Network)
- **GDAL** 3.x (optional, for GIS format support)
- **GEOS** 3.x (optional, for geometry operations)
- **C++17** compatible compiler (GCC 9+, Clang 10+, MSVC 2019+)

### Quick Start (Linux/macOS)

Use the automated quick start script:

```bash
git clone https://github.com/ConsoleMangena/sitesurveyor.git
cd sitesurveyor
./scripts/quickstart.sh
```

The script auto-detects your OS, installs dependencies, and builds the project.

### Linux Build

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install build-essential cmake qt6-base-dev libgdal-dev libgeos-dev

# Clone and build
git clone https://github.com/ConsoleMangena/sitesurveyor.git
cd sitesurveyor
mkdir build && cd build
cmake .. -DWITH_GDAL=ON -DWITH_GEOS=ON
make -j$(nproc)

./bin/SiteSurveyor
```

### Windows Build

```powershell
# Install Qt 6.6+, CMake, and vcpkg
vcpkg install gdal:x64-windows geos:x64-windows

cmake -B build -DCMAKE_TOOLCHAIN_FILE=[vcpkg-path]/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

### macOS Build

```bash
brew install cmake qt@6 gdal geos
cmake -B build -DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)
cmake --build build
```

### Docker Build

Build and run in a container:

```bash
# Build Docker image
docker build -t sitesurveyor .

# Run with X11 forwarding (Linux)
docker run -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix sitesurveyor
```

### CMake Presets

For standardized builds, use [CMake Presets](CMakePresets.json):

```bash
# List available presets
cmake --list-presets

# Configure and build with a preset
cmake --preset linux-release
cmake --build --preset linux-release
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `WITH_GDAL` | ON | Enable GDAL support for GIS formats |
| `WITH_GEOS` | ON | Enable GEOS for geometry operations |
| `BUNDLE_GIS_LIBS` | ON | Bundle GIS libraries with the application |

## Project Structure

```
sitesurveyor/
├── src/                    # Source files
│   ├── app/                # Application core
│   ├── auth/               # Authentication & cloud
│   ├── canvas/             # Map rendering
│   ├── categories/         # Survey categories
│   ├── gama/               # GNU Gama integration
│   ├── gdal/               # GDAL/GEOS integration
│   └── tools/              # Survey calculation tools
├── include/                # Header files
├── resources/              # Icons, themes, templates
├── external/               # Third-party dependencies
└── CMakeLists.txt          # Build configuration
```

## Dependencies

SiteSurveyor uses the following open-source libraries:

| Library | License | Purpose |
|---------|---------|---------|
| [Qt6](https://www.qt.io/) | LGPL | GUI framework |
| [GDAL](https://gdal.org/) | MIT | Geospatial data I/O |
| [GEOS](https://libgeos.org/) | LGPL | Geometry operations |
| [PROJ](https://proj.org/) | MIT | Coordinate transformations |
| [GNU Gama](https://www.gnu.org/software/gama/) | GPL | Network adjustment |
| [Qt Advanced Docking](https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System) | LGPL | Dockable panels |

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

Please read our [Code of Conduct](CODE_OF_CONDUCT.md) before contributing.

## License

This project is licensed under the terms specified in the [LICENSE](LICENSE) file.

## Acknowledgments

- The GNU Gama project for network adjustment capabilities
- The GDAL/OGR team for geospatial data support
- The Qt team for the excellent cross-platform framework

---

<p align="center">
  <strong>Eineva Incorporated</strong><br>
  <a href="https://sitesurveyor.dev">sitesurveyor.dev</a>
</p>
