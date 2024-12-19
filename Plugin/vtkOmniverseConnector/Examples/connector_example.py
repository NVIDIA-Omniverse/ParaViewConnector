#!/usr/bin/env python

# This simple example shows how to do basic rendering and pipeline
# creation.

import sys
from vtkmodules.vtkCommonColor import vtkNamedColors
from vtkmodules.vtkImagingCore import vtkRTAnalyticSource
from vtkmodules.vtkFiltersCore import vtkContourFilter
from vtkmodules.vtkRenderingCore import (
    vtkActor,
    vtkPolyDataMapper,
    vtkRenderWindow,
    vtkRenderWindowInteractor,
    vtkRenderer
)
from vtkmodules.vtkCommonCore import (
    vtkInformation,
    vtkDataArraySelection
)
from vtkmodules.vtkCommonDataModel import vtkDataObject
from vtkmodules.vtkOmniverseConnector import (
    vtkOmniConnectPass,
    vtkOmniConnectSettings,
    vtkOmniConnectEnvironment,
    vtkOmniConnectRendererNode,
    vtkOmniConnectPassArrays
)


def main():
    if len(sys.argv) < 2:
        exit()

    usd_output_dir = sys.argv[1]

    colors = vtkNamedColors()

    # Create renderers and windows
    ren = vtkRenderer()
    renWin = vtkRenderWindow()
    renWin.AddRenderer(ren)

    # Create a wavelet source
    wavelet = vtkRTAnalyticSource()
    wavelet.SetWholeExtent(-10, 10, -10, 10, -10, 10)
    wavelet.Update()

    # Create a contour filter
    contour = vtkContourFilter()
    contour.SetInputConnection(wavelet.GetOutputPort())
    contour.GenerateValues(5, 0.0, 200.0)  # Generate 5 contours between 0 and 200
    contour.Update()

    # Set up a vtkPassArrays filter, which is just a wrapped version of the vtkPassSelectedArrays filter
    # that additionally tells the VTK Omniverse Connector to output the selected
    # auxiliary data arrays along with the data used for rendering.
    # Using this filter, the 'RTData' array will be added as a per-vertex primvar attribute 
    # of the generated mesh geometry, which can be found back in USD with the name 'primvars:vtk_point_RTData'
    passArrays = vtkOmniConnectPassArrays()
    passArrays.SetInputConnection(contour.GetOutputPort())
    dataArraySelection = passArrays.GetPointDataArraySelection()
    dataArraySelection.EnableArray("RTData")

    # Create a polygonal mapper
    waveletMapper = vtkPolyDataMapper()
    waveletMapper.SetInputConnection(passArrays.GetOutputPort())

    # Create an actor
    waveletActor = vtkActor()
    waveletActor.SetMapper(waveletMapper)
    waveletActor.GetProperty().SetColor(colors.GetColor3d("Tomato"))
    waveletActor.RotateX(30.0)
    waveletActor.RotateY(-45.0)

    ren.AddActor(waveletActor)



    ## VTK Omniverse Connector specific code

    # Every actor requires a name in order to produce valid USD output
    actorProperties = vtkInformation()
    actorProperties.Set(vtkOmniConnectRendererNode.ACTORNAME(), "Testwavelet")
    waveletActor.SetPropertyKeys(actorProperties)

    # Set an output directory for the USD files
    omniConnectSettings = vtkOmniConnectSettings()
    omniConnectSettings.SetOutputLocal(1)
    omniConnectSettings.SetLocalOutputDirectory( usd_output_dir )
    # To enable this, set SetOutputLocal(0)
    #omniConnectSettings.SetOmniWorkingDirectory('/Users/test/vtk')
    #omniConnectSettings.SetOmniServer('localhost')

    # Create the connector render pass, where the initial timestep is also required
    omniConnectPass = vtkOmniConnectPass()
    omniConnectPass.Initialize(omniConnectSettings, vtkOmniConnectEnvironment(), 0.0)

    # The scenegraph (vtkRendererNode) allows for certain settings that can be changed
    # between multiple invocations of Render(), where content is actually written out to USD
    # This is where the desired output timestep of the currently converted data can be changed (see below).
    sceneGraph = omniConnectPass.GetSceneGraph()
    sceneGraph.SetRenderingEnabled(False)

    ren.SetPass(omniConnectPass)

    ## ~ VTK Omniverse Connector specific code



    # 'Render' the scene, which will write the polygonal data to USD
    renWin.Render()



    ## Subsequent timesteps

    # Output a slightly transformed actor for timestep 1.0.
    waveletActor.RotateZ(-5.0)
    actorProperties.Set(vtkDataObject.DATA_TIME_STEP(), 1.0)
    waveletActor.SetPropertyKeys(actorProperties)

    # The 'scene timestep' is controlled independently of the actor timestep, so that one can
    # explicitly differentiate actors with changes for a particular timestep from those that stay the same across multiple scene timesteps.
    # This allows for reuse of data in combination with more complicated animation patterns, where actors can choose on an individual basis
    # which part of their state is shared (stays the same) across scene timesteps, by linking individual actor timesteps to one or multiple scene timesteps.
    sceneGraph.SetSceneTime(1.0)
    renWin.Render()

if __name__ == '__main__':
    main()
