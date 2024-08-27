message( "-------------------- Begin OmniConnectConnection CmakeLists.txt ----------------------------------------")

option(OMNICONNECT_USE_OMNIVERSE "Enable Omniverse support for Omniverse Connector" ON)

project(OmniConnect_Connect)
add_library(${PROJECT_NAME} INTERFACE)

####################
# Omniverse Client #
####################

function(get_omni_dependency
  DEP_ROOT_DIR
  DEP_INCLUDE_DIR
  DEP_LIB_DIR
  INPUT_ROOT_DIR
  INPUT_ROOT_DIR_NAME
  INPUT_NAME
  INPUT_HEADER)

  if (NOT EXISTS ${INPUT_ROOT_DIR})
    find_path(INPUT_ROOT_DIR NAMES ${INPUT_HEADER} DOC "Path to ${INPUT_NAME}")
    message(STATUS "Using ${INPUT_ROOT_DIR_NAME}: ${INPUT_ROOT_DIR}")
  endif()

  if (NOT EXISTS ${INPUT_ROOT_DIR})
    message(FATAL_ERROR "No valid ${INPUT_ROOT_DIR_NAME} set, or found using CMAKE_PREFIX_PATH: ${INPUT_ROOT_DIR}")
  endif()

  set(${DEP_ROOT_DIR} ${INPUT_ROOT_DIR} PARENT_SCOPE)

  if(EXISTS ${INPUT_ROOT_DIR})
    MESSAGE(STATUS "OmniConnect is using ${INPUT_NAME} at: ${INPUT_ROOT_DIR}")

    set(DEP_DIR_RELEASE "${INPUT_ROOT_DIR}/release")
    set(DEP_DIR_DEBUG "${INPUT_ROOT_DIR}/debug")
    set(DEP_LIB_DIR_RELEASE "${DEP_DIR_RELEASE}")
    set(DEP_LIB_DIR_DEBUG "${DEP_DIR_DEBUG}")

    set(${DEP_INCLUDE_DIR} "${INPUT_ROOT_DIR}/include" PARENT_SCOPE)

    if (CMAKE_CONFIGURATION_TYPES)
      set(_DEP_LIB_DIR "$<$<NOT:$<CONFIG:Debug>>:${DEP_LIB_DIR_RELEASE}>$<$<CONFIG:Debug>:${DEP_LIB_DIR_DEBUG}>")
    else()
      if(CMAKE_BUILD_TYPE MATCHES "Debug")
        set(_DEP_LIB_DIR ${DEP_LIB_DIR_DEBUG})
      else()
        set(_DEP_LIB_DIR ${DEP_LIB_DIR_RELEASE})
      endif()
    endif()

    set(${DEP_LIB_DIR} ${_DEP_LIB_DIR} PARENT_SCOPE)

    MESSAGE(STATUS "${INPUT_NAME} LIB_DIR: ${_DEP_LIB_DIR}")
  else()
    MESSAGE(FATAL_ERROR "Cannot find ${INPUT_ROOT_DIR_NAME}: ${INPUT_ROOT_DIR}")
  endif()

endfunction()

if(${OMNICONNECT_USE_OMNIVERSE})

  get_omni_dependency(
    OMNICLIENT_ROOT_DIR
    OMNICLIENT_INCLUDE_DIR
    OMNICLIENT_LIB_DIR
    "${OMNICLIENT_ROOT_DIR}"
    "OMNICLIENT_ROOT_DIR"
    "Omniverse Client"
    "include/OmniClient.h")

  get_omni_dependency(
    OMNIUSDRESOLVER_ROOT_DIR
    OMNIUSDRESOLVER_INCLUDE_DIR
    OMNIUSDRESOLVER_LIB_DIR
    "${OMNIUSDRESOLVER_ROOT_DIR}"
    "OMNIUSDRESOLVER_ROOT_DIR"
    "Omniverse USD Resolver"
    "include/OmniUsdResolver.h")

  list(APPEND OMNICLIENT_SHARED_LIBS omniclient)
  list(APPEND OMNIUSDRESOLVER_SHARED_LIBS omni_usd_resolver)

else()
    MESSAGE(STATUS "OmniConnect is NOT using OmniClient for remote connections.")
endif()

#####################
# OmniConnectConnection #
#####################

target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_sources(${PROJECT_NAME}
  INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/OmniConnectConnection.cxx
    ${CMAKE_CURRENT_LIST_DIR}/OmniConnectConnection.h
)

target_link_libraries(${PROJECT_NAME}
  INTERFACE  
    OmniConnect_Common
)

if(${OMNICONNECT_USE_OMNIVERSE})
  target_include_directories(${PROJECT_NAME} INTERFACE ${OMNICLIENT_INCLUDE_DIR} ${OMNIUSDRESOLVER_INCLUDE_DIR})
  target_compile_definitions(${PROJECT_NAME} INTERFACE OMNIVERSE_CONNECTION_ENABLE)
  
  if (WIN32)
    target_link_directories(${PROJECT_NAME} INTERFACE ${OMNICLIENT_LIB_DIR} ${OMNIUSDRESOLVER_LIB_DIR})
    set(OMNICONNECT_CONNECTION_LINK_LIBS ${OMNICLIENT_SHARED_LIBS} ${OMNIUSDRESOLVER_SHARED_LIBS})
  else()
    target_compile_definitions(${PROJECT_NAME} INTERFACE
      _GLIBCXX_USE_CXX11_ABI=0 )

    foreach(lib ${OMNICLIENT_SHARED_LIBS})
      find_library(path ${lib} PATHS ${OMNICLIENT_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
      list(APPEND OMNI_LIBS ${path})
      unset(path CACHE)
    endforeach()

    foreach(lib ${OMNIUSDRESOLVER_SHARED_LIBS})
      find_library(path ${lib} PATHS ${OMNIUSDRESOLVER_LIB_DIR} NO_DEFAULT_PATH REQUIRED)
      list(APPEND OMNI_LIBS ${path})
      unset(path CACHE)
    endforeach()
  
    set(OMNICONNECT_CONNECTION_LINK_LIBS ${OMNI_LIBS})
  endif()

  target_link_libraries(${PROJECT_NAME} INTERFACE ${OMNICONNECT_CONNECTION_LINK_LIBS})

  option(INSTALL_OMNIVERSE_DEPS "Enable install of Omniverse dependencies with Omniverse Connector" OFF)
  if(${INSTALL_OMNIVERSE_DEPS})
    # Install client library
    set(PLATFORM_INSTALL_LIBDIR "$<$<NOT:$<PLATFORM_ID:Windows>>:${CMAKE_INSTALL_LIBDIR}>$<$<PLATFORM_ID:Windows>:${CMAKE_INSTALL_BINDIR}>/")
    install(
      DIRECTORY "${OMNICLIENT_LIB_DIR}/"
      DESTINATION ${PLATFORM_INSTALL_LIBDIR}
      PATTERN "*${CMAKE_STATIC_LIBRARY_SUFFIX}" EXCLUDE)
    install(
      DIRECTORY "${OMNIUSDRESOLVER_LIB_DIR}/"
      DESTINATION ${PLATFORM_INSTALL_LIBDIR}
      PATTERN "*${CMAKE_STATIC_LIBRARY_SUFFIX}" EXCLUDE)
  endif()
endif()

message( "-------------------- End OmniConnectConnection CmakeLists.txt ----------------------------------------")

