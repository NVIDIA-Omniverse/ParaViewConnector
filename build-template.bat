@echo off
REM Windows batch file template for building ParaView Omniverse Connector
REM Edit the paths below to match your installation directories

REM ======================== CONFIGURATION ========================
REM Edit these paths to match your installation
set PARAVIEW_DIR=C:\path\to\paraview\lib\cmake\paraview-<pv_version>
set USD_ROOT_DIR=C:\path\to\usd
set QT6_DIR=C:\path\to\qt\lib\cmake\Qt6
set PYTHON_EXECUTABLE=C:\path\to\python.exe
set PYTHON_INCLUDE_DIR=C:\path\to\python\include
set PYTHON_LIBRARY=C:\path\to\python\libs\python3x.lib
set FREETYPE_INSTALLATION=C:\path\to\freetype
set FREETYPE_LIBRARY=C:\path\to\freetype\bin\freetype.lib
set INSTALL_PREFIX=C:\path\to\install
set SOURCE_DIR=%~dp0
set BUILD_DIR=%~dp0build

REM ======================== BUILD PROCESS ========================

echo Configuring CMake...
cmake -S "%SOURCE_DIR%" -B "%BUILD_DIR%" ^
  -D ParaView_DIR="%PARAVIEW_DIR%" ^
  -D USD_ROOT_DIR="%USD_ROOT_DIR%" ^
  -D OMNICONNECT_USE_OMNIVERSE=OFF ^
  -D OMNICONNECT_USE_OPENVDB=OFF ^
  -D Qt6_DIR="%QT6_DIR%" ^
  -D Python3_EXECUTABLE="%PYTHON_EXECUTABLE%" ^
  -D Python3_INCLUDE_DIR="%PYTHON_INCLUDE_DIR%" ^
  -D Python3_LIBRARY="%PYTHON_LIBRARY%" ^
  -D CMAKE_PREFIX_PATH="%FREETYPE_INSTALLATION%" ^
  -D FREETYPE_LIBRARY="%FREETYPE_LIBRARY%" ^
  -D FREETYPE_INCLUDE_DIRS="%FREETYPE_INSTALLATION%\include" ^
  -D FREETYPE_INCLUDE_DIR_ft2build="%FREETYPE_INSTALLATION%\include" ^
  -D FREETYPE_INCLUDE_DIR_freetype2="%FREETYPE_INSTALLATION%\include" ^
  -D INSTALL_USD_DEPS=ON

if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Building project...
cmake --build "%BUILD_DIR%" --parallel --config Release

if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Installing to: %INSTALL_PREFIX%
cmake --install "%BUILD_DIR%" --prefix "%INSTALL_PREFIX%" --config Release

if %ERRORLEVEL% neq 0 (
    echo Install failed!
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Build completed successfully!
echo Installation location: %INSTALL_PREFIX%
pause
