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

#include "vtkOmniConnectTemporalArrays.h"

#include "vtkObjectFactory.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkDemandDrivenPipeline.h"
#include "vtkUnsignedLongLongArray.h"
#include "vtkDoubleArray.h"
#include "vtkFieldData.h"
#include "vtkDataArraySelection.h"
#include "OmniConnectData.h"

vtkStandardNewMacro(vtkOmniConnectTemporalArrays);

namespace
{
  void CopyEnabledArraysToOutput(const vtkDataArraySelection* selectedArrays, vtkUnsignedLongLongArray* omnipassFlag, int& currentIdx)
  {
    for (int i = 0; i < selectedArrays->GetNumberOfArrays(); ++i)
    {
      if (selectedArrays->GetArraySetting(i))
      {
        omnipassFlag->SetValue(currentIdx, (long long int)(selectedArrays->GetArrayName(i)));
        ++currentIdx;
      }
    }
  }
}

const char* vtkOmniConnectTemporalArrays::TemporalArraysFlagName = "OmniConnectTemporalArraysFlag";

vtkOmniConnectTemporalArrays::vtkOmniConnectTemporalArrays()
{
}

vtkOmniConnectTemporalArrays::~vtkOmniConnectTemporalArrays()
{
}

//----------------------------------------------------------------------------
int vtkOmniConnectTemporalArrays::RequestData(
  vtkInformation*,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  // Get the info objects
  vtkInformation *inInfo = inputVector[0]->GetInformationObject(0);
  vtkInformation *outInfo = outputVector->GetInformationObject(0);

  // Get the input and output objects
  vtkDataObject* inputData = inInfo->Get(vtkDataObject::DATA_OBJECT());
  vtkDataObject* outputData = outInfo->Get(vtkDataObject::DATA_OBJECT());
  outputData->ShallowCopy(inputData);

  //vtkInformation* outputDataInfo = outputData->GetInformation();
  vtkFieldData* fieldData = outputData->GetFieldData();

  vtkUnsignedLongLongArray* omnipassFlag = vtkUnsignedLongLongArray::New();
  static_assert(sizeof(long long int) == sizeof(const char*), "Long long int and pointer type are different (building in 32 bit?)");
  omnipassFlag->SetName(TemporalArraysFlagName);

  vtkDataArraySelection* pointArrays = this->GetPointDataArraySelection();
  vtkDataArraySelection* cellArrays = this->GetCellDataArraySelection();
  int numberOfEnabledArrays = pointArrays->GetNumberOfArraysEnabled() + cellArrays->GetNumberOfArraysEnabled();

  int currArrayIdx = (int)OmniConnectGeomType::NUM_GEOMTYPES;
  omnipassFlag->SetNumberOfValues(numberOfEnabledArrays + currArrayIdx);

  typedef OmniConnectMeshData::DataMemberId TMeshUpdate;
  typedef OmniConnectInstancerData::DataMemberId TInstancerUpdate;
  typedef OmniConnectCurveData::DataMemberId TCurveUpdate;
  typedef OmniConnectVolumeData::DataMemberId TVolumeUpdate;
  TMeshUpdate meshUpdates = 
    (AllowMeshPoints ? TMeshUpdate::POINTS : TMeshUpdate::NONE) |
    (AllowMeshIndices ? TMeshUpdate::INDICES : TMeshUpdate::NONE) |
    (AllowMeshNormals ? TMeshUpdate::NORMALS : TMeshUpdate::NONE) |
    (AllowMeshTexcoords ? TMeshUpdate::TEXCOORDS : TMeshUpdate::NONE) |
    (AllowMeshVertexColors ? TMeshUpdate::COLORS : TMeshUpdate::NONE);
  TInstancerUpdate instancerUpdates = 
    (AllowPointsPositions ? TInstancerUpdate::POINTS : TInstancerUpdate::NONE) |
    //TInstancerUpdate::SHAPEINDICES | // Updates managed by the polydatamapper as shape type is fixed, never timevarying
    (AllowPointsIds ? TInstancerUpdate::INSTANCEIDS : TInstancerUpdate::NONE) |
    (AllowPointsScales ? TInstancerUpdate::SCALES : TInstancerUpdate::NONE) |
    (AllowPointsOrientations ? TInstancerUpdate::ORIENTATIONS : TInstancerUpdate::NONE) |
    (AllowPointsTexcoords ? TInstancerUpdate::TEXCOORDS : TInstancerUpdate::NONE) |
    (AllowPointsVertexColors ? TInstancerUpdate::COLORS : TInstancerUpdate::NONE);
  TCurveUpdate curveUpdates = 
    (AllowCurvePoints ? TCurveUpdate::POINTS : TCurveUpdate::NONE) |
    (AllowCurveNormals ? TCurveUpdate::NORMALS : TCurveUpdate::NONE) |
    (AllowCurveWidths ? TCurveUpdate::SCALES : TCurveUpdate::NONE) |
    (AllowCurveTexcoords ? TCurveUpdate::TEXCOORDS : TCurveUpdate::NONE) |
    (AllowCurveVertexColors ? TCurveUpdate::COLORS : TCurveUpdate::NONE) |
    (AllowCurveLengths ? TCurveUpdate::CURVELENGTHS : TCurveUpdate::NONE);
  TVolumeUpdate volumeUpdates = (AllowVolumeData ? TVolumeUpdate::DATA : TVolumeUpdate::NONE);

  omnipassFlag->SetValue((int)OmniConnectMeshData::GeomType, (unsigned long long) meshUpdates);
  omnipassFlag->SetValue((int)OmniConnectInstancerData::GeomType, (unsigned long long) instancerUpdates);
  omnipassFlag->SetValue((int)OmniConnectCurveData::GeomType, (unsigned long long) curveUpdates);
  omnipassFlag->SetValue((int)OmniConnectVolumeData::GeomType, (unsigned long long) volumeUpdates);
  
  CopyEnabledArraysToOutput(pointArrays, omnipassFlag, currArrayIdx);
  CopyEnabledArraysToOutput(cellArrays, omnipassFlag, currArrayIdx);

  fieldData->AddArray(omnipassFlag);
  omnipassFlag->Delete();

  return 1;
}

//----------------------------------------------------------------------------
void vtkOmniConnectTemporalArrays::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
