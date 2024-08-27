message( "-------------------- Begin OmniConnectCommon CmakeLists.txt ----------------------------------------")

project(OmniConnect_Common)
add_library(${PROJECT_NAME} INTERFACE)

target_sources(${PROJECT_NAME}
  INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/OmniConnectUtilsInternal.cxx
    ${CMAKE_CURRENT_LIST_DIR}/OmniConnectUtilsInternal.h
    ${CMAKE_CURRENT_LIST_DIR}/OmniConnectData.h
)

target_include_directories(${PROJECT_NAME} INTERFACE ${CMAKE_CURRENT_LIST_DIR})

message( "-------------------- End OmniConnectCommon CmakeLists.txt ----------------------------------------")

