#!/bin/bash

# Ensure we are in the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Configuration
INSTALLER_NAME="SiteSurveyorInstaller"
OFFLINE_INSTALLER="${INSTALLER_NAME}_Offline"
ONLINE_INSTALLER="${INSTALLER_NAME}_Online"
CONFIG_DIR="config"
PACKAGES_DIR="packages"


# Check for binarycreator
BINARYCREATOR="binarycreator"

# Check local installation first
LOCAL_BC="../Tools/QtInstallerFramework/4.7/bin/binarycreator"
if [ -f "$LOCAL_BC" ]; then
    BINARYCREATOR="$LOCAL_BC"
    echo "Using local binarycreator: $BINARYCREATOR"
elif ! command -v binarycreator &> /dev/null; then
    echo "Error: 'binarycreator' tool from Qt Installer Framework not found."
    echo "Please install it or use 'aqt install-tool linux desktop tools_ifw qt.tools.ifw.47'"
    exit 1
fi

# Check if data directory has files
DATA_DIR="packages/com.consolemangena.sitesurveyor/data"
if [ -z "$(ls -A $DATA_DIR)" ]; then
    echo "Warning: Data directory '$DATA_DIR' is empty."
    echo "You need to copy your application binaries and dependencies into this folder before building."
    echo "Usage example:"
    echo "  cp -r ../build/bin/* $DATA_DIR/"
    echo "  # (And don't forget to run windeployqt or linuxdeployqt on the binary first!)"
    read -p "Do you want to continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

echo "Building offline installer..."
"$BINARYCREATOR" --offline-only -c "$CONFIG_DIR/config.xml" -p "$PACKAGES_DIR" "$OFFLINE_INSTALLER"


if [ $? -eq 0 ]; then
    echo "Success! Installer created: $OFFLINE_INSTALLER"
else
    echo "Error: Failed to create installer."
    exit 1
fi
