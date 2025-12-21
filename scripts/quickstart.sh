#!/bin/bash
# SiteSurveyor Quick Start Script for Linux/macOS
# This script sets up the development environment and builds the project

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}╔═══════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║         SiteSurveyor Quick Start Script               ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════════════╝${NC}"
echo ""

# Detect OS
OS="unknown"
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
    if [ -f /etc/debian_version ]; then
        DISTRO="debian"
    elif [ -f /etc/fedora-release ]; then
        DISTRO="fedora"
    elif [ -f /etc/arch-release ]; then
        DISTRO="arch"
    else
        DISTRO="unknown"
    fi
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
fi

echo -e "${GREEN}Detected OS:${NC} $OS"
if [ "$OS" == "linux" ]; then
    echo -e "${GREEN}Detected Distribution:${NC} $DISTRO"
fi
echo ""

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check and install dependencies
install_dependencies() {
    echo -e "${YELLOW}Checking dependencies...${NC}"
    
    if [ "$OS" == "linux" ]; then
        case $DISTRO in
            debian)
                echo -e "${BLUE}Installing dependencies via apt...${NC}"
                sudo apt-get update
                sudo apt-get install -y \
                    build-essential \
                    cmake \
                    ninja-build \
                    git \
                    qt6-base-dev \
                    libgdal-dev \
                    libgeos-dev \
                    libproj-dev \
                    libgl1-mesa-dev
                ;;
            fedora)
                echo -e "${BLUE}Installing dependencies via dnf...${NC}"
                sudo dnf install -y \
                    gcc-c++ \
                    cmake \
                    ninja-build \
                    git \
                    qt6-qtbase-devel \
                    gdal-devel \
                    geos-devel \
                    proj-devel \
                    mesa-libGL-devel
                ;;
            arch)
                echo -e "${BLUE}Installing dependencies via pacman...${NC}"
                sudo pacman -S --needed --noconfirm \
                    base-devel \
                    cmake \
                    ninja \
                    git \
                    qt6-base \
                    gdal \
                    geos \
                    proj
                ;;
            *)
                echo -e "${RED}Unknown Linux distribution. Please install dependencies manually.${NC}"
                echo "Required: cmake, ninja, qt6, gdal, geos, proj"
                exit 1
                ;;
        esac
    elif [ "$OS" == "macos" ]; then
        # Check for Homebrew
        if ! command_exists brew; then
            echo -e "${YELLOW}Installing Homebrew...${NC}"
            /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        fi
        
        echo -e "${BLUE}Installing dependencies via Homebrew...${NC}"
        brew install cmake ninja qt@6 gdal geos proj
    else
        echo -e "${RED}Unsupported operating system.${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}Dependencies installed successfully!${NC}"
    echo ""
}

# Initialize submodules
init_submodules() {
    echo -e "${YELLOW}Initializing git submodules...${NC}"
    git submodule update --init --recursive
    echo -e "${GREEN}Submodules initialized!${NC}"
    echo ""
}

# Configure and build
build_project() {
    local BUILD_TYPE="${1:-Release}"
    local BUILD_DIR="build"
    
    echo -e "${YELLOW}Configuring build (${BUILD_TYPE})...${NC}"
    
    # Set Qt path for macOS
    if [ "$OS" == "macos" ]; then
        CMAKE_PREFIX="-DCMAKE_PREFIX_PATH=$(brew --prefix qt@6)"
    else
        CMAKE_PREFIX=""
    fi
    
    cmake -B "$BUILD_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DWITH_GDAL=ON \
        -DWITH_GEOS=ON \
        $CMAKE_PREFIX
    
    echo -e "${YELLOW}Building...${NC}"
    cmake --build "$BUILD_DIR" --parallel $(nproc 2>/dev/null || sysctl -n hw.ncpu)
    
    echo ""
    echo -e "${GREEN}╔═══════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║              Build completed successfully!            ║${NC}"
    echo -e "${GREEN}╚═══════════════════════════════════════════════════════╝${NC}"
    echo ""
    
    if [ "$OS" == "macos" ]; then
        echo -e "Run with: ${BLUE}open build/bin/SiteSurveyor.app${NC}"
    else
        echo -e "Run with: ${BLUE}./build/bin/SiteSurveyor${NC}"
    fi
}

# Main script
main() {
    # Parse arguments
    SKIP_DEPS=false
    BUILD_TYPE="Release"
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --skip-deps)
                SKIP_DEPS=true
                shift
                ;;
            --debug)
                BUILD_TYPE="Debug"
                shift
                ;;
            --release)
                BUILD_TYPE="Release"
                shift
                ;;
            --help|-h)
                echo "Usage: $0 [OPTIONS]"
                echo ""
                echo "Options:"
                echo "  --skip-deps    Skip dependency installation"
                echo "  --debug        Build in Debug mode"
                echo "  --release      Build in Release mode (default)"
                echo "  --help, -h     Show this help message"
                exit 0
                ;;
            *)
                echo -e "${RED}Unknown option: $1${NC}"
                exit 1
                ;;
        esac
    done
    
    # Run steps
    if [ "$SKIP_DEPS" = false ]; then
        install_dependencies
    fi
    
    init_submodules
    build_project "$BUILD_TYPE"
}

# Run main
main "$@"
