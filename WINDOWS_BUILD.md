# Building SiteSurveyor for Windows

Because SiteSurveyor relies on GIS libraries (GDAL, GEOS) which are complex to cross-compile, the most reliable way to build for Windows is using a Windows environment (or a comprehensive Docker setup like MXE, which takes hours to build initially).

## Option 1: Build on Windows (Recommended)

1.  **Install Tools**:
    *   **Qt 6.6+** (MinGW 64-bit component)
    *   **CMake**
    *   **Ninja** (optional, faster)
    *   **Vcpkg** (for GDAL/GEOS)

2.  **Install Dependencies with Vcpkg**:
    ```powershell
    git clone https://github.com/microsoft/vcpkg
    .\vcpkg\bootstrap-vcpkg.bat
    .\vcpkg\vcpkg install gdal:x64-windows geos:x64-windows
    ```

3.  **Build SiteSurveyor**:
    ```powershell
    cmake -B build -G "MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE=[path/to/vcpkg/scripts/buildsystems/vcpkg.cmake]
    cmake --build build --config Release
    ```

4.  **Create Installer**:
    *   Copy `installer/` folder to your Windows machine.
    *   Copy `build/Release/SiteSurveyor.exe` to `installer/packages/com.consolemangena.sitesurveyor/data/`.
    *   Run `windeployqt installer/packages/com.consolemangena.sitesurveyor/data/SiteSurveyor.exe`.
    *   Run `binarycreator.exe -c config/config.xml -p packages MyInstaller.exe`.

## Option 2: Docker Cross-Compile (Linux)

I have included a `Dockerfile.windows` that uses `aqtinstall` to fetch Qt.
However, it currently **excludes** GDAL/GEOS unless you uncomment the vcpkg section (which adds ~30-60 mins to the build).

To run it:
```bash
cd docker
./build.sh windows
```
*Note: This might fail if compilation requires GDAL symbols.*

## Current Status (Linux Environment)

*   **Linux Installer**: BUILT (`installer/SiteSurveyorInstaller_Offline`).
*   **Windows Installer**: Configured (`installer/config/config.xml`), but requires Windows binaries.
