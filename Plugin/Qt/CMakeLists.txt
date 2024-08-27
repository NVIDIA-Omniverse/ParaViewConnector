message( "-------------------- Begin OmniverseConnectorQt CmakeLists.txt ----------------------------------------")

project(OmniverseConnectorQt)
cmake_minimum_required(VERSION 3.3)

set(CMAKE_CXX_STANDARD 17)

IF(PARAVIEW_USE_QT)

  SET(MOC_SOURCES
	  pqOmniConnectStarter.h
	  pqOmniConnectStarter.cxx
	  pqOmniConnectActions.h
	  pqOmniConnectActions.cxx
	  pqOmniConnectBaseDialog.h
	  pqOmniConnectBaseDialog.cxx
	  pqOmniConnectAccountDialog.h
	  pqOmniConnectAccountDialog.cxx
	  pqOmniConnectSettingsDialog.h
	  pqOmniConnectSettingsDialog.cxx
	  pqOmniConnectAboutDialog.h
	  pqOmniConnectAboutDialog.cxx
	  pqOmniConnectFolderPickerDialog.h
	  pqOmniConnectFolderPickerDialog.cxx
	  pqOmniConnectDataItem.h
	  pqOmniConnectDataItem.cxx
	  pqOmniConnectFolderPickerTreeModel.h
	  pqOmniConnectFolderPickerTreeModel.cxx
	  pqOmniConnectPropertyLinks.h
	  pqOmniConnectPropertyLinks.cxx
	  pqOmniConnectViewsSettingsManager.h
	  pqOmniConnectViewsSettingsManager.cxx
	  pqOmniConnectLauncherClient.h
	  pqOmniConnectLauncherClient.cxx
	  pqOmniConnectViewOpener.h
	  pqOmniConnectViewOpener.cxx
	  pqOmniConnectViewClient.h
	  pqOmniConnectViewClient.cxx
	  pqOmniConnectUtils.h
	  pqOmniConnectUtils.cxx
	  pqOmniConnectFileLogger.h
	  pqOmniConnectFileLogger.cxx
	  pqOmniConnectLogger.h
	  pqOmniConnectLogger.cxx
    pqOmniConnectConnectionDialog.h
    pqOmniConnectConnectionDialog.cxx
    )
  SET(MOC_IFACES)

  paraview_plugin_add_auto_start(
    CLASS_NAME
      pqOmniConnectStarter
    STARTUP
      onStartup
    SHUTDOWN
      onShutdown
    INTERFACES 
      OMNICONNECT_AUTOSTART_IFACE
    SOURCES 
      OMNICONNECT_AUTOSTART_IFACE_SRCS
      )
  list(APPEND MOC_IFACES
    ${OMNICONNECT_AUTOSTART_IFACE})
  list(APPEND MOC_SOURCES
    ${OMNICONNECT_AUTOSTART_IFACE_SRCS})

  paraview_plugin_add_toolbar(
    CLASS_NAME 
      "pqOmniConnectActions"
    INTERFACES 
      OMNICONNECT_TBAR_IFACE
    SOURCES 
      OMNICONNECT_TBAR_IFACE_SRCS
      )
  list(APPEND MOC_IFACES
    ${OMNICONNECT_TBAR_IFACE})
  list(APPEND MOC_SOURCES
    ${OMNICONNECT_TBAR_IFACE_SRCS})
  
  set(ui_files
	Resources/UI/pqOmniConnectAccountDialog.ui
	Resources/UI/pqOmniConnectSettingsDialog.ui
	Resources/UI/pqOmniConnectAboutDialog.ui
	Resources/UI/pqOmniConnectFolderPickerDialog.ui)

  set(resource_files
    Resources/omniComponents.qrc)

  find_package(Qt5 REQUIRED QUIET COMPONENTS Network)

  set(CMAKE_AUTOMOC 1)
  set(CMAKE_AUTORCC 1)
  set(CMAKE_AUTOUIC 1)
  set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/UI")

  source_group("Resources" FILES
    ${ui_files}
    ${resource_files})

ENDIF()

paraview_add_plugin(OmniverseConnectorQt
  REQUIRED_ON_CLIENT
  VERSION "1.0"
  UI_FILES
    ${ui_files}
  UI_RESOURCES
    ${resource_files}
  UI_INTERFACES 
    ${MOC_IFACES}
  SOURCES 
    ${MOC_SOURCES}
  MODULES
  MODULE_FILES 
  XML_DOCUMENTATION OFF
  )
  
target_include_directories( OmniverseConnectorQt PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/> )

target_link_libraries(OmniverseConnectorQt 
  PRIVATE 
    OmniverseConnector::vtkOmniverseConnector
    OmniverseConnector::OmniConnectViews
    Qt5::Network)

message( "-------------------- End OmniverseConnectorQt CmakeLists.txt ----------------------------------------")

