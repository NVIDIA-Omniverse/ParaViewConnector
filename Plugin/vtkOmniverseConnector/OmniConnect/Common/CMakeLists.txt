message( "-------------------- Begin OmniConnectCommon CmakeLists.txt ----------------------------------------")

project(OmniConnect_Common)
add_library(${PROJECT_NAME} INTERFACE)

target_sources(${PROJECT_NAME}
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/OmniConnectUtilsInternal.cxx>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/OmniConnectUtilsInternal.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/OmniConnectData.h>
)

target_include_directories(${PROJECT_NAME}
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    $<INSTALL_INTERFACE:include>
  )

message( "-------------------- End OmniConnectCommon CmakeLists.txt ----------------------------------------")

