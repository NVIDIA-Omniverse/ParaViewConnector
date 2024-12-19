message( "-------------------- Begin vtkOmniverseConnector CmakeLists.txt ----------------------------------------")

cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(OmniConnect)

if(DEFINED PARAVIEW_VTK_OMNIVERSE_CONNECTOR_BUILD)
  set(OMNIVERSE_CONNECTOR_MODULE_NAME "OmniverseConnector::vtkOmniverseConnector")
else()
  set(OMNIVERSE_CONNECTOR_MODULE_NAME "VTK::vtkOmniverseConnector")
endif()

#################
# vtkOmniverseConnector #
#################

vtk_module_add_module(${OMNIVERSE_CONNECTOR_MODULE_NAME}
  CLASSES 
    vtkOmniConnectPass
    vtkOmniConnectViewNodeFactory
    vtkOmniConnectRendererNode
    vtkOmniConnectActorNode
    vtkOmniConnectVolumeNode
    vtkOmniConnectPolyDataMapperNode
    vtkOmniConnectCompositePolyDataMapperNode
    vtkOmniConnectVolumeMapperNode
    vtkOmniConnectMultiBlockVolumeMapperNode
    vtkOmniConnectImageSliceMapperNode
    vtkOmniConnectGlyph3DMapperNode
    vtkOmniConnectPassArrays
    vtkOmniConnectTemporalArrays
    vtkOmniConnectUsdExporter
  HEADERS
    vtkOmniConnectVtkToOmni.h
  SOURCES
    vtkOmniConnectVtkToOmni.cxx
    vtkOmniConnectMapperNodeCommon.cxx
    vtkOmniConnectActorNodeBase.cxx
    vtkOmniConnectTimeStep.cxx
    vtkOmniConnectActorCache.cxx
    vtkOmniConnectTextureCache.cxx
    vtkOmniConnectLogCallback.cxx
    vtkOmniConnectImageWriter.cxx
  PRIVATE_HEADERS
    vtkOmniConnectMapperNodeCommon.h
    vtkOmniConnectActorNodeBase.h
    vtkOmniConnectTimeStep.h
    vtkOmniConnectActorCache.h
    vtkOmniConnectTextureCache.h
    vtkOmniConnectImageWriter.h
    vtkOmniConnectLogCallback.h
    )

if(DEFINED PARAVIEW_VTK_OMNIVERSE_CONNECTOR_BUILD)
  vtk_module_definitions(${OMNIVERSE_CONNECTOR_MODULE_NAME}
    PRIVATE
      PARAVIEW_VTK_OMNIVERSE_CONNECTOR_BUILD
    )
endif()

_vtk_module_install(OmniConnect_Common)
_vtk_module_install(OmniConnect_Connect)
_vtk_module_install(OmniConnect_Volume)
_vtk_module_install(OmniConnect)

vtk_module_link(${OMNIVERSE_CONNECTOR_MODULE_NAME}
  PUBLIC
    OmniConnect
  )

message( "-------------------- End vtkOmniverseConnector CmakeLists.txt ----------------------------------------")

