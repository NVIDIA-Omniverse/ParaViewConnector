#!/bin/bash
# Linux shell script template for building ParaView Omniverse Connector
# Edit the paths below to match your installation directories

# ======================== CONFIGURATION ========================
# Edit these paths to match your installation
PARAVIEW_DIR="/path/to/paraview/lib/cmake/paraview-<pv_version>"
USD_ROOT_DIR="/path/to/usd"
QT6_DIR="/path/to/qt/lib/cmake/Qt6"
PYTHON_EXECUTABLE="/path/to/python3/bin/python3"
PYTHON_INCLUDE_DIR="/path/to/python3/include"
PYTHON_LIBRARY="/path/to/python3/lib/libpython3.x.so"
FREETYPE_INSTALLATION="/path/to/freetype"
FREETYPE_LIBRARY="/path/to/freetype/lib/libfreetype.so"
INSTALL_PREFIX="/path/to/install"
SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SOURCE_DIR}/build"

# ======================== BUILD PROCESS ========================

# Function to check command success
check_error() {
    if [ $? -ne 0 ]; then
        echo "Error: $1 failed!"
        exit 1
    fi
}

echo "Configuring CMake..."
cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" \
  -D ParaView_DIR="${PARAVIEW_DIR}" \
  -D USD_ROOT_DIR="${USD_ROOT_DIR}" \
  -D OMNICONNECT_USE_OMNIVERSE=OFF \
  -D OMNICONNECT_USE_OPENVDB=OFF \
  -D Qt6_DIR="${QT6_DIR}" \
  -D Python3_EXECUTABLE="${PYTHON_EXECUTABLE}" \
  -D Python3_INCLUDE_DIR="${PYTHON_INCLUDE_DIR}" \
  -D Python3_LIBRARY="${PYTHON_LIBRARY}" \
  -D CMAKE_PREFIX_PATH="${FREETYPE_INSTALLATION}" \
  -D FREETYPE_LIBRARY="${FREETYPE_LIBRARY}" \
  -D FREETYPE_INCLUDE_DIRS="${FREETYPE_INSTALLATION}/include" \
  -D FREETYPE_INCLUDE_DIR_ft2build="${FREETYPE_INSTALLATION}/include" \
  -D FREETYPE_INCLUDE_DIR_freetype2="${FREETYPE_INSTALLATION}/include" \
  -D INSTALL_USD_DEPS=ON

check_error "CMake configuration"

echo ""
echo "Building project..."
cmake --build "${BUILD_DIR}" --parallel

check_error "Build"

echo ""
echo "Installing to: ${INSTALL_PREFIX}"
cmake --install "${BUILD_DIR}" --prefix "${INSTALL_PREFIX}"

check_error "Install"

echo ""
echo "Build completed successfully!"
echo "Installation location: ${INSTALL_PREFIX}"
echo ""
echo "Note: Make sure the installed connector plugin and dependency binaries"
echo "are available in ParaView's plugin path or system PATH."
