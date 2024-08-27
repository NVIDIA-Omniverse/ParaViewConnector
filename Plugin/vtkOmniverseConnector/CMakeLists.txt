message( "-------------------- Begin vtkOmniverseConnector CmakeLists.txt ----------------------------------------")

cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(OmniConnect)

#################
# vtkOmniverseConnector #
#################

vtk_module_add_module(OmniverseConnector::vtkOmniverseConnector
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

target_link_libraries(vtkOmniverseConnector
  PUBLIC
    OmniConnect
  )

message( "-------------------- End vtkOmniverseConnector CmakeLists.txt ----------------------------------------")

