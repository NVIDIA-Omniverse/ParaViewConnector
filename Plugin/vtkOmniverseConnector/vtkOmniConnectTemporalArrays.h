/*###############################################################################
#
# Copyright 2024 NVIDIA Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
###############################################################################*/

#ifndef vtkOmniConnectTemporalArrays_h
#define vtkOmniConnectTemporalArrays_h

#include "vtkOmniverseConnectorModule.h" // For export macro
#include "vtkPassSelectedArrays.h"

class VTKOMNIVERSECONNECTOR_EXPORT vtkOmniConnectTemporalArrays : public vtkPassSelectedArrays
{
public:
  static vtkOmniConnectTemporalArrays* New();
  vtkTypeMacro(vtkOmniConnectTemporalArrays, vtkPassSelectedArrays);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  // Meshes

  vtkSetMacro(AllowMeshPoints, bool);
  vtkGetMacro(AllowMeshPoints, bool);
  vtkBooleanMacro(AllowMeshPoints, bool);

  vtkSetMacro(AllowMeshIndices, bool);
  vtkGetMacro(AllowMeshIndices, bool);
  vtkBooleanMacro(AllowMeshIndices, bool);

  vtkSetMacro(AllowMeshNormals, bool);
  vtkGetMacro(AllowMeshNormals, bool);
  vtkBooleanMacro(AllowMeshNormals, bool);

  vtkSetMacro(AllowMeshTexcoords, bool);
  vtkGetMacro(AllowMeshTexcoords, bool);
  vtkBooleanMacro(AllowMeshTexcoords, bool);

  vtkSetMacro(AllowMeshVertexColors, bool);
  vtkGetMacro(AllowMeshVertexColors, bool);
  vtkBooleanMacro(AllowMeshVertexColors, bool);

  //Points / Sticks

  vtkSetMacro(AllowPointsPositions, bool);
  vtkGetMacro(AllowPointsPositions, bool);
  vtkBooleanMacro(AllowPointsPositions, bool);

  vtkSetMacro(AllowPointsIds, bool);
  vtkGetMacro(AllowPointsIds, bool);
  vtkBooleanMacro(AllowPointsIds, bool);

  vtkSetMacro(AllowPointsScales, bool);
  vtkGetMacro(AllowPointsScales, bool);
  vtkBooleanMacro(AllowPointsScales, bool);

  vtkSetMacro(AllowPointsOrientations, bool);
  vtkGetMacro(AllowPointsOrientations, bool);
  vtkBooleanMacro(AllowPointsOrientations, bool);

  vtkSetMacro(AllowPointsTexcoords, bool);
  vtkGetMacro(AllowPointsTexcoords, bool);
  vtkBooleanMacro(AllowPointsTexcoords, bool);

  vtkSetMacro(AllowPointsVertexColors, bool);
  vtkGetMacro(AllowPointsVertexColors, bool);
  vtkBooleanMacro(AllowPointsVertexColors, bool);

  //Curves

  vtkSetMacro(AllowCurvePoints, bool);
  vtkGetMacro(AllowCurvePoints, bool);
  vtkBooleanMacro(AllowCurvePoints, bool);

  vtkSetMacro(AllowCurveLengths, bool);
  vtkGetMacro(AllowCurveLengths, bool);
  vtkBooleanMacro(AllowCurveLengths, bool);

  vtkSetMacro(AllowCurveNormals, bool);
  vtkGetMacro(AllowCurveNormals, bool);
  vtkBooleanMacro(AllowCurveNormals, bool);

  vtkSetMacro(AllowCurveWidths, bool);
  vtkGetMacro(AllowCurveWidths, bool);
  vtkBooleanMacro(AllowCurveWidths, bool);

  vtkSetMacro(AllowCurveTexcoords, bool);
  vtkGetMacro(AllowCurveTexcoords, bool);
  vtkBooleanMacro(AllowCurveTexcoords, bool);

  vtkSetMacro(AllowCurveVertexColors, bool);
  vtkGetMacro(AllowCurveVertexColors, bool);
  vtkBooleanMacro(AllowCurveVertexColors, bool);

  // Volumes

  vtkSetMacro(AllowVolumeData, bool);
  vtkGetMacro(AllowVolumeData, bool);
  vtkBooleanMacro(AllowVolumeData, bool);

  static const char* TemporalArraysFlagName; 

protected:
  vtkOmniConnectTemporalArrays();
  ~vtkOmniConnectTemporalArrays();

  int RequestData(
    vtkInformation*,
    vtkInformationVector**,
    vtkInformationVector*) override;

  bool AllowMeshPoints = true;
  bool AllowMeshIndices = true;
  bool AllowMeshNormals = true;
  bool AllowMeshTexcoords = true;
  bool AllowMeshVertexColors = true;
  bool AllowPointsPositions = true;
  bool AllowPointsIds = true;
  bool AllowPointsScales = true;
  bool AllowPointsOrientations = true;
  bool AllowPointsTexcoords = true;
  bool AllowPointsVertexColors = true;
  bool AllowCurvePoints = true;
  bool AllowCurveLengths = true;
  bool AllowCurveNormals = true;
  bool AllowCurveWidths = true;
  bool AllowCurveTexcoords = true;
  bool AllowCurveVertexColors = true;
  bool AllowVolumeData = true;

private:
  vtkOmniConnectTemporalArrays(const vtkOmniConnectTemporalArrays&) = delete;
  void operator=(const vtkOmniConnectTemporalArrays&) = delete;
};

#endif
