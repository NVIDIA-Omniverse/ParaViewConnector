paraview_add_plugin(OmniverseConnector
  REQUIRED_ON_CLIENT
  REQUIRED_ON_SERVER
  VERSION "1.0"
  UI_FILES 
  UI_INTERFACES 
  SOURCES 
  MODULES
    OmniverseConnector::vtkOmniverseConnector
    OmniverseConnector::OmniConnectViews
  MODULE_FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/vtkOmniverseConnector/vtk.module"
    "${CMAKE_CURRENT_SOURCE_DIR}/Views/vtk.module"
  XML_DOCUMENTATION OFF
  )
  
target_include_directories(OmniverseConnector PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/> )

target_link_libraries(OmniverseConnector PUBLIC OmniConnect)

paraview_plugin_scan(
  PLUGIN_FILES      "${CMAKE_CURRENT_SOURCE_DIR}/Qt/paraview.sub.plugin"
  PROVIDES_PLUGINS  qtplugin
  REQUIRES_MODULES  required_modules
  ENABLE_BY_DEFAULT ON)

MESSAGE(STATUS "Qt Sub plugin found with name: ${qtplugin}")

paraview_plugin_build(
  RUNTIME_DESTINATION   "${CMAKE_INSTALL_BINDIR}"
  LIBRARY_DESTINATION   "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY_SUBDIRECTORY  "${PARAVIEW_PLUGIN_SUBDIR}/OmniverseConnector"
  PLUGINS ${qtplugin})


