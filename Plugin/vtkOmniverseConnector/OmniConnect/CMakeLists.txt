message( "-------------------- Begin OmniConnect CmakeLists.txt ----------------------------------------")

if (WIN32)
cmake_minimum_required(VERSION 3.15 FATAL_ERROR)
endif()

set(CMAKE_CXX_STANDARD 17)

include(usdhelpers.cmake)

if(DEFINED PARAVIEW_VTK_OMNIVERSE_CONNECTOR_BUILD)
  set(OMNICONNECT_RUNTIME_DESTINATION ${_paraview_build_plugin_directory})
else()
  set(OMNICONNECT_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

##########
# Python #
##########

list(PREPEND CMAKE_MODULE_PATH ${Python3_ROOT_DIR})
find_package(Python3 REQUIRED COMPONENTS Development)
list(POP_FRONT CMAKE_MODULE_PATH)

set(USD_PYTHON_TARGETS Python3::Module)
set(PY_SHARED_LIB_DIR "${Python3_RUNTIME_LIBRARY_DIRS}")
set(USD_PYTHON_LINK_DIRS "${Python3_LIBRARY_DIRS}")
list(GET Python3_INCLUDE_DIRS 0 Python3_INCLUDE_DIR) # Required for passing pxrConfig.cmake
list(GET Python3_LIBRARIES 0 Python3_LIBRARY) # Required for passing pxrConfig.cmake

#######
# USD #
#######

if (NOT EXISTS ${USD_ROOT_DIR})
  find_path(USD_ROOT_DIR NAMES include/pxr/pxr.h DOC "Path to USD")
  message(STATUS "Using USD_ROOT_DIR: ${USD_ROOT_DIR}")
endif()

if (NOT EXISTS ${USD_ROOT_DIR})
  message(FATAL_ERROR "No valid USD_ROOT_DIR set, or found using CMAKE_PREFIX_PATH: ${USD_ROOT_DIR}")
endif()

message(STATUS "Using cmake value of USD_ROOT_DIR: ${USD_ROOT_DIR}")
if(EXISTS "${USD_ROOT_DIR}/release")
  set(USD_ROOT_DIR_RELEASE "${USD_ROOT_DIR}/release")
else()
  set(USD_ROOT_DIR_RELEASE "${USD_ROOT_DIR}")
endif()
if(EXISTS "${USD_ROOT_DIR}/debug")
  set(USD_ROOT_DIR_DEBUG "${USD_ROOT_DIR}/debug")
else()
  set(USD_ROOT_DIR_DEBUG "${USD_ROOT_DIR}")
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  set(USD_ROOT_DIR_CURRENT "${USD_ROOT_DIR_RELEASE}")
else()
  set(USD_ROOT_DIR_CURRENT "${USD_ROOT_DIR_DEBUG}")
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  if(NOT EXISTS "${USD_ROOT_DIR_DEBUG}/include")
    message(FATAL_ERROR "USD_ROOT_DIR or its /debug subfolder does not have an /include subfolder, so it's not configured correctly at ${USD_ROOT_DIR_DEBUG}")
  endif()
elseif(NOT EXISTS "${USD_ROOT_DIR_RELEASE}/include")
  message(FATAL_ERROR "USD_ROOT_DIR or its /release subfolder does not have an /include subfolder, so it's not configured correctly at ${USD_ROOT_DIR_RELEASE}")
endif()

set(USD_DIR "$<$<NOT:$<CONFIG:Debug>>:${USD_ROOT_DIR_RELEASE}>$<$<CONFIG:Debug>:${USD_ROOT_DIR_DEBUG}>")
if(CMAKE_CONFIGURATION_TYPES)
  set(USD_LIB_DIR "${USD_DIR}/lib")
else()
  # Generator-expression-free, due to find_library later on
  if(CMAKE_BUILD_TYPE MATCHES "Debug")
    set(USD_LIB_DIR "${USD_ROOT_DIR_DEBUG}/lib")
  else()
    set(USD_LIB_DIR "${USD_ROOT_DIR_RELEASE}/lib")
  endif()
endif()

if(NOT DEFINED Imath_DIR) # Transitive deps
  set(Imath_DIR "${USD_ROOT_DIR_CURRENT}/lib64/cmake/Imath")
  if(NOT EXISTS ${Imath_DIR})
    set(Imath_DIR "${USD_ROOT_DIR_CURRENT}/lib/cmake/Imath")
  endif()
endif()
if(NOT DEFINED MaterialX_DIR)
  set(MaterialX_DIR "${USD_ROOT_DIR_CURRENT}/lib/cmake/MaterialX")
endif()

# Search in config mode
find_package(pxr REQUIRED
  PATHS ${USD_ROOT_DIR_CURRENT})

if(NOT (USD_ROOT_DIR_DEBUG STREQUAL USD_ROOT_DIR_RELEASE))
  set(USD_DEBUG_TARGET_CMAKE "${USD_ROOT_DIR_DEBUG}/cmake/pxrTargets-debug.cmake")
  if(EXISTS ${USD_DEBUG_TARGET_CMAKE})
    set(_IMPORT_PREFIX ${USD_ROOT_DIR_DEBUG})
    #include USD debug cmake targets
    include(${USD_DEBUG_TARGET_CMAKE})
    set(_IMPORT_PREFIX)
  endif()
endif()

list(APPEND USD_INIT_TARGETS usdGeom usdVol usdShade usdLux usdUtils) # All other targets derived from these, in flatten_and_cleanup_targets
flatten_and_cleanup_targets(USD_TARGETS "${USD_INIT_TARGETS}")
message(STATUS "USD_TARGETS: ${USD_TARGETS}")

# Find out whether USD was compiled with or without the CXX11 ABI
if(NOT WIN32)
  set(CXX11_COMPILE_OFF OFF)
  check_file_and_substring(
    "${USD_ROOT_DIR_CURRENT}/BUILD_INFO/CMakeCache.txt"
    "-D_GLIBCXX_USE_CXX11_ABI=0"
    CXX11_COMPILE_OFF)

  if(${CXX11_COMPILE_OFF})
    set(USE_CXX11_ABI_DEFAULT OFF)
  else()
    set(USE_CXX11_ABI_DEFAULT ON)
  endif()

  option(USD_DEVICE_USE_CXX11_ABI "" ${USE_CXX11_ABI_DEFAULT})
endif()

##########
# Boost #
##########

file(GLOB Boost_SEP_INCLUDE_DIR "${USD_ROOT_DIR_RELEASE}/include/boost*")
message(STATUS "Boost_SEP_INCLUDE_DIR ${Boost_SEP_INCLUDE_DIR}")

##############
# OmniConnect #
##############

add_subdirectory(Common)
add_subdirectory(Mdl)
add_subdirectory(Connection)
add_subdirectory(Volume)

project(OmniConnect)
add_library(${PROJECT_NAME} SHARED)

target_sources(${PROJECT_NAME}
  PRIVATE
    OmniConnect.cxx
    OmniConnectLiveInterface.cxx
    OmniConnectUtilsExternal.cxx
    OmniConnectUsdUtils.cxx
    OmniConnectCaches.cxx
    OmniConnectDiagnosticMgrDelegate.cxx
    ${OMNICONNECT_MDL_SOURCES}
    OmniConnectUtilsExternal.h
    OmniConnectUsdUtils.h
    OmniConnect.h
    OmniConnectLiveInterface.h
    OmniConnectCaches.h
    OmniConnectDiagnosticMgrDelegate.h
    ${OMNICONNECT_MDL_HEADERS}
    usd.h
)
  
target_include_directories(${PROJECT_NAME} PRIVATE
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/Mdl
    ${Boost_SEP_INCLUDE_DIR}
  )

if (WIN32)
  add_definitions("-DBOOST_ALL_DYN_LINK")
  # Combination of boost and python causes pragma linking issues for both libraries.
  message(STATUS "Python link dirs: ${USD_PYTHON_LINK_DIRS}")
  target_link_directories(${PROJECT_NAME} PRIVATE "${USD_PYTHON_LINK_DIRS}" "${USD_LIB_DIR}")
else()
  set(PLATFORM_LIBS pthread dl stdc++fs)
  target_link_libraries(${PROJECT_NAME} PRIVATE general ${PLATFORM_LIBS} )
  if(NOT ${USD_DEVICE_USE_CXX11_ABI})
    target_compile_definitions(${PROJECT_NAME} PRIVATE _GLIBCXX_USE_CXX11_ABI=0 )
  endif()
endif()

target_link_libraries(${PROJECT_NAME}
  PUBLIC  
    OmniConnect_Common
  PRIVATE
    OmniConnect_Volume
    OmniConnect_Connect
    ${USD_TARGETS}
    ${USD_PYTHON_TARGETS}
)

##############
# Install    #
##############

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${OMNICONNECT_RUNTIME_DESTINATION}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )

option(INSTALL_USD_DEPS "Enable install of USD dependencies with Omniverse Connector" OFF)
if(${INSTALL_USD_DEPS})

  set(PLATFORM_INSTALL_LIBDIR "$<$<NOT:$<PLATFORM_ID:Windows>>:${CMAKE_INSTALL_LIBDIR}>$<$<PLATFORM_ID:Windows>:${CMAKE_INSTALL_BINDIR}>/")

  #Install USD
  install(
    DIRECTORY "${USD_LIB_DIR}/"
    DESTINATION ${PLATFORM_INSTALL_LIBDIR}
    PATTERN "*${CMAKE_STATIC_LIBRARY_SUFFIX}" EXCLUDE)
  install(
    DIRECTORY "$<$<NOT:$<CONFIG:Debug>>:${USD_ROOT_DIR_RELEASE}/bin/>$<$<CONFIG:Debug>:${USD_ROOT_DIR_DEBUG}/bin/>"
    DESTINATION ${PLATFORM_INSTALL_LIBDIR}
    FILES_MATCHING PATTERN "*${CMAKE_SHARED_LIBRARY_SUFFIX}*")

  #Install Python
  message(STATUS "Python runtime library dirs: ${PY_SHARED_LIB_DIR}")
  if(WIN32)
    file(GLOB PYTHON_RUNTIME_LIBRARIES "${PY_SHARED_LIB_DIR}/python*${CMAKE_SHARED_LIBRARY_SUFFIX}*")
  else()
    file(GLOB PYTHON_RUNTIME_LIBRARIES
      "${PY_SHARED_LIB_DIR}/*libffi.so*"
      "${PY_SHARED_LIB_DIR}/*libpython*.so*")
  endif()
  message(STATUS "Found Python Runtime Libs: ${PYTHON_RUNTIME_LIBRARIES}")
  install(
      FILES ${PYTHON_RUNTIME_LIBRARIES}
      DESTINATION ${PLATFORM_INSTALL_LIBDIR})
endif()

message( "-------------------- End OmniConnect CmakeLists.txt ----------------------------------------")

