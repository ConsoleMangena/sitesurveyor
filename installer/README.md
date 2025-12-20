# SiteSurveyor Installer (Qt Installer Framework)

This directory contains the configuration files to generate a professional installer for SiteSurveyor using the [Qt Installer Framework](https://doc.qt.io/qtinstallerframework/).

## Prerequisites

1.  **Build the Application**: Compile SiteSurveyor.
2.  **Qt Installer Framework**: Download and install it from [Qt.io](https://download.qt.io/official_releases/qt-installer-framework/).
3.  **Add to PATH**: Ensure `binarycreator` is in your system PATH.

## Structure

```
installer/
├── config/
│   └── config.xml           # Main installer settings
├── packages/
│   └── com.consolemangena.sitesurveyor/
│       ├── data/            # PUT YOUR APP BINARIES HERE
│       └── meta/
│           ├── package.xml  # Component metadata
│           ├── license.txt  # License agreement
│           └── installscript.qs # Shortcut creation script
└── build_installer.sh       # Helper script to generate installer
```

## How to Create the Installer

1.  **Populate the Data Directory**:
    Copy your compiled application and all dependencies (Qt DLLs, plugins, etc.) into the `data` folder.
    
    *Windows Example:*
    ```bash
    # Assuming you built in ../build/release
    cp ../build/release/SiteSurveyor.exe installer/packages/com.consolemangena.sitesurveyor/data/
    # Run windeployqt to copy dependencies
    windeployqt installer/packages/com.consolemangena.sitesurveyor/data/SiteSurveyor.exe
    ```

    *Linux Example:*
    ```bash
    cp ../build/bin/SiteSurveyor installer/packages/com.consolemangena.sitesurveyor/data/
    # Use linuxdeployqt or manually copy .so files
    ```

2.  **Run the Build Script**:
    ```bash
    cd installer
    ./build_installer.sh
    ```

3.  **Result**: 
    You will get an installer executable (e.g., `SiteSurveyorInstaller_Offline`) in this directory.
