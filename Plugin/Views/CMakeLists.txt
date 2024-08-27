set(CMAKE_CXX_STANDARD 17)

vtk_module_add_module(OmniverseConnector::OmniConnectViews
  CLASSES 
    vtkPVOmniConnectRenderView
    vtkPVOmniConnectSettings
    vtkPVOmniConnectSettingsPlaceholder
    vtkPVOmniConnectGlobalState
    vtkPVOmniConnectProxy
    vtkPVOmniConnectProxyUrlInfo
    vtkPVOmniConnectAnimProxy
    vtkPVOmniConnectSMPipelineController
    vtkPVOmniConnectUsdExporter
    vtkSMOmniConnectRenderViewProxy
  SOURCES
    vtkPVOmniConnectNamesManager.cxx
  PRIVATE_HEADERS
    vtkPVOmniConnectNamesManager.h)

paraview_add_server_manager_xmls(
  XMLS  OmniConnectViews.xml)

target_compile_definitions(OmniConnectViews PRIVATE
   "PARAVIEW_SUBDIR=\"${PARAVIEW_PLUGIN_SUBDIR}\""
  )
