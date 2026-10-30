message( "-------------------- Begin OmniConnectVolume CmakeLists.txt ----------------------------------------")

# Boost's iostreams cmake config file tacks on generic z and lzma dep names
macro(fix_boost_dependency)
  if(NOT WIN32 AND BOOST_USE_STATIC_LIBS)
    find_package(Boost REQUIRED COMPONENTS iostreams)
    get_property(Boost_linkiface
      TARGET Boost::iostreams
      PROPERTY INTERFACE_LINK_LIBRARIES)

    foreach(libname ${Boost_linkiface})
      if(CMAKE_CONFIGURATION_TYPES)
        if(NOT (${libname} MATCHES "$<$<CONFIG:release>:lzma;z>"))
          list(APPEND Boost_linkiface_new ${libname})
        else()
          message(STATUS "Removed boost dependency: ${libname}")
        endif()
      else()
        if((NOT (${libname} MATCHES "lzma")) AND
          (NOT (${libname} MATCHES "z")))
          list(APPEND Boost_linkiface_new ${libname})
        else()
          message(STATUS "Removed boost dependency: ${libname}")
        endif()
      endif()
    endforeach(libname)

    message(STATUS "CHANGED Boost iostreams link interface to: ${Boost_linkiface_new}")
    set_property(TARGET Boost::iostreams
      PROPERTY INTERFACE_LINK_LIBRARIES ${Boost_linkiface_new})
  endif()
endmacro()

project(OmniConnect_Volume)
add_library(${PROJECT_NAME} SHARED)

option(OMNICONNECT_USE_OPENVDB "Enable OpenVDB support for USD device" OFF)
option(OMNICONNECT_VOLUME_STATIC_CRT "Enable static CRT on Volume part of OmniConnect" OFF)

# In case there is no openvdb support, simply compile a stub
target_sources(${PROJECT_NAME}
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/OmniConnectVolumeWriter.cxx
    ${CMAKE_CURRENT_LIST_DIR}/OmniConnectVolumeWriter.h
)

target_include_directories(${PROJECT_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${PROJECT_NAME}
  PRIVATE  
    OmniConnect_Common
)

###########
# OpenVDB #
###########

if(${OMNICONNECT_USE_OPENVDB})

  if(NOT CMAKE_BUILD_TYPE) # Otherwise IlmBase::Half does not find debug/release libraries
    message(FATAL_ERROR "A CMAKE_BUILD_TYPE has to be chosen for correct dependency resolution of OmniConnect_Volume")
  endif()

  # OpenVDB install dir
  
  file(GLOB USD_OPENVDB_BUILD "${USD_INSTALL_DIR_RELEASE}/include/openvdb")
  option(USE_USD_OPENVDB_BUILD "If USD source installation exists, use OpenVDB from there" ON)

  # Find path with Find*.cmake files that find_package() will use for deps

  unset(OPENVDB_MODULE_DIR CACHE)
  find_path(OPENVDB_MODULE_DIR
    PATHS
      ${OpenVDB_ROOT}
    NAMES
      FindOpenVDB.cmake
    PATH_SUFFIXES
      OpenVDB
      cmake/OpenVDB
      lib/cmake/OpenVDB
  )
  # Also expose the parent OmniConnect/cmake dir so FindOpenVDB's internal
  # find_package(TBB) can locate the local FindTBB.cmake. A dynamically-built
  # OpenVDB package may ship no CMake modules of its own, so without this the
  # lookup falls back to CONFIG mode and fails when no TBBConfig.cmake is provided.
  list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake" "${CMAKE_CURRENT_LIST_DIR}/../cmake" ${OPENVDB_MODULE_DIR})

  # Set dep package roots (or manual lib dirs) and find variables

  # No consensus on upper/lower case
  if(Boost_USE_STATIC_LIBS OR BOOST_USE_STATIC_LIBS)
    set(BOOST_USE_STATIC_LIBS ON)
    set(Boost_USE_STATIC_LIBS ON)
    set(BUILD_SHARED_LIBS OFF)
  else()
    set(BOOST_USE_STATIC_LIBS OFF)
    set(Boost_USE_STATIC_LIBS OFF)
    set(BUILD_SHARED_LIBS ON)
  endif()

  if(USE_USD_OPENVDB_BUILD) # Allow for using the usd install as dependency root
    if(NOT ZLIB_ROOT)
      set(ZLIB_ROOT "${OpenVDB_ROOT}")
    endif()
    if(NOT TBB_ROOT)
      set(TBB_ROOT "${OpenVDB_ROOT}")
    endif()
    if(NOT BOOST_ROOT)
      set(BOOST_ROOT "${OpenVDB_ROOT}")
    endif()
    if(NOT Blosc_ROOT)
      set(Blosc_ROOT "${OpenVDB_ROOT}")
    endif()
  endif()

  # Some builds need OpenVDB to link against a TBB installation that differs
  # from the project-wide TBB dependency. Keep the OpenVDB-specific TBB
  # dependency private to the volume library instead of reusing the global
  # TBB::tbb target.
  set(_OPENVDB_TBB_LIBRARY_DIRS "")
  set(_OPENVDB_TBB_RUNTIME_DIR "")
  if(OPENVDB_TBB_ROOT)
    find_path(OPENVDB_TBB_INCLUDE_DIR
      NAMES tbb/tbb_stddef.h
      PATHS "${OPENVDB_TBB_ROOT}"
      PATH_SUFFIXES include
      NO_DEFAULT_PATH)

    if(CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug$")
      set(_OPENVDB_TBB_LIBRARY_NAMES tbb_debug tbb)
      set(_OPENVDB_TBB_RUNTIME_NAMES tbb_debug.dll tbb.dll)
    else()
      set(_OPENVDB_TBB_LIBRARY_NAMES tbb)
      set(_OPENVDB_TBB_RUNTIME_NAMES tbb.dll)
    endif()

    find_library(OPENVDB_TBB_LIBRARY
      NAMES ${_OPENVDB_TBB_LIBRARY_NAMES}
      PATHS "${OPENVDB_TBB_ROOT}"
      PATH_SUFFIXES lib lib64
      NO_DEFAULT_PATH)

    if(WIN32)
      find_path(OPENVDB_TBB_RUNTIME_DIR
        NAMES ${_OPENVDB_TBB_RUNTIME_NAMES}
        PATHS "${OPENVDB_TBB_ROOT}"
        PATH_SUFFIXES bin
        NO_DEFAULT_PATH)
      set(_OPENVDB_TBB_RUNTIME_DIR "${OPENVDB_TBB_RUNTIME_DIR}")
    endif()

    if(NOT OPENVDB_TBB_INCLUDE_DIR OR NOT OPENVDB_TBB_LIBRARY)
      message(FATAL_ERROR
        "OPENVDB_TBB_ROOT is set to ${OPENVDB_TBB_ROOT}, but the TBB "
        "headers/libraries required by the static OpenVDB package were not found")
    endif()

    get_filename_component(_OPENVDB_TBB_LIBRARY_DIR "${OPENVDB_TBB_LIBRARY}" DIRECTORY)
    list(APPEND _OPENVDB_TBB_LIBRARY_DIRS "${_OPENVDB_TBB_LIBRARY_DIR}")

    if(NOT TARGET OpenVDBPrivateTBB::tbb)
      add_library(OpenVDBPrivateTBB::tbb UNKNOWN IMPORTED)
      set_target_properties(OpenVDBPrivateTBB::tbb PROPERTIES
        IMPORTED_LOCATION "${OPENVDB_TBB_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${OPENVDB_TBB_INCLUDE_DIR}"
        INTERFACE_LINK_DIRECTORIES "${_OPENVDB_TBB_LIBRARY_DIR}")
    endif()
    set(OPENVDB_TBB_TARGET OpenVDBPrivateTBB::tbb)
  endif()

  message(STATUS "OpenVDB OpenVDB_ROOT: ${OpenVDB_ROOT}")
  message(STATUS "OpenVDB IlmBase_ROOT: ${IlmBase_ROOT}")
  message(STATUS "OpenVDB ZLIB_ROOT: ${ZLIB_ROOT}")
  message(STATUS "OpenVDB Blosc_ROOT: ${Blosc_ROOT}")
  message(STATUS "OpenVDB TBB_ROOT: ${TBB_ROOT}")
  message(STATUS "OpenVDB OPENVDB_TBB_ROOT: ${OPENVDB_TBB_ROOT}")
  message(STATUS "OpenVDB OPENVDB_TBB_TARGET: ${OPENVDB_TBB_TARGET}")
  message(STATUS "OpenVDB BOOST_ROOT: ${BOOST_ROOT}")

  if(OMNICONNECT_VOLUME_STATIC_CRT)
    set(Boost_USE_STATIC_RUNTIME ON)
  else()
    set(Boost_USE_STATIC_RUNTIME OFF)
  endif()
  if (WIN32)
    set(Boost_COMPILER "vc141")
  endif()

  fix_boost_dependency()

  find_package(OpenVDB REQUIRED)

  # Pop three paths to remove current dir, parent cmake dir and OPENVDB_MODULE_DIR
  list(POP_FRONT CMAKE_MODULE_PATH)
  list(POP_FRONT CMAKE_MODULE_PATH)
  list(POP_FRONT CMAKE_MODULE_PATH)
  
  ####################
  # OmniConnectVolume #
  ####################

  target_link_libraries(${PROJECT_NAME}
  PRIVATE
    OpenVDB::openvdb
  )

  target_compile_definitions(${PROJECT_NAME}
  PRIVATE
    USE_OPENVDB
  )

  if(WIN32)
    # Due to explicit pragma lib inclusion in tbb and python
    target_link_directories(${PROJECT_NAME}
      PRIVATE ${Tbb_LIBRARY_DIRS} ${_OPENVDB_TBB_LIBRARY_DIRS})

    set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/OmniConnectVolumeWriter.cxx PROPERTIES COMPILE_FLAGS /bigobj)
  else()
  endif()

endif() # OMNICONNECT_USE_OPENVDB

if(WIN32)
  if(OMNICONNECT_VOLUME_STATIC_CRT)
    set_property(TARGET ${PROJECT_NAME}
      PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
else()
  if(NOT ${USD_DEVICE_USE_CXX11_ABI})
    target_compile_definitions(${PROJECT_NAME} PRIVATE
      _GLIBCXX_USE_CXX11_ABI=0 )
  endif()
endif()

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${OMNICONNECT_RUNTIME_DESTINATION}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )

if(OMNICONNECT_USE_OPENVDB AND NOT OPENVDB_USE_STATIC_LIBS AND INSTALL_USD_DEPS)
  if(WIN32)
    install(DIRECTORY "${OpenVDB_ROOT}/bin/"
      DESTINATION ${OMNICONNECT_RUNTIME_DESTINATION}
      FILES_MATCHING PATTERN "openvdb*${CMAKE_SHARED_LIBRARY_SUFFIX}")
  else()
    install(DIRECTORY "${OpenVDB_ROOT}/lib/"
      DESTINATION ${CMAKE_INSTALL_LIBDIR}
      FILES_MATCHING PATTERN "libopenvdb*${CMAKE_SHARED_LIBRARY_SUFFIX}*")
  endif()
endif()

if(OMNICONNECT_USE_OPENVDB AND OPENVDB_USE_STATIC_LIBS AND INSTALL_USD_DEPS AND OPENVDB_TBB_ROOT)
  if(WIN32 AND _OPENVDB_TBB_RUNTIME_DIR)
    install(DIRECTORY "${_OPENVDB_TBB_RUNTIME_DIR}/"
      DESTINATION ${OMNICONNECT_RUNTIME_DESTINATION}
      FILES_MATCHING PATTERN "tbb*${CMAKE_SHARED_LIBRARY_SUFFIX}")
  elseif(UNIX)
    install(DIRECTORY "${OPENVDB_TBB_ROOT}/lib/"
      DESTINATION ${CMAKE_INSTALL_LIBDIR}
      FILES_MATCHING PATTERN "libtbb*${CMAKE_SHARED_LIBRARY_SUFFIX}*")
  endif()
endif()

message( "-------------------- End OmniConnectVolume CmakeLists.txt ----------------------------------------")
