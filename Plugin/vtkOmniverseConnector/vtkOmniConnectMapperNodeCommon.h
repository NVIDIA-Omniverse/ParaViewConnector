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

#ifndef vtkOmniConnectMapperNodeCommon_h
#define vtkOmniConnectMapperNodeCommon_h

#include "vtkOmniConnectTemporalArrays.h"
#include "OmniConnectData.h"
#include "vtkUnsignedLongLongArray.h"
#include "vtkFieldData.h"
#include "vtkOmniConnectActorNodeBase.h"

class vtkDataSet;
class OmniConnect;

void SetUpdatesToPerform(OmniConnectMaterialData& omniMatData, bool forceAllUpdates);

void SetConvertGenericArraysDoubleToFloat(OmniConnect* connector, vtkDataSet* dataSet, bool defaultValue);

void DirectionToQuaternionX(float* dir, float dirLength, float* quat);
void DirectionToQuaternionY(float* dir, float dirLength, float* quat);
void RotationToQuaternion(float* rot, float* quat);

template<class OmniConnectData, class VtkData>
void SetUpdatesToPerform(OmniConnectData& omniGeomData, VtkData* vtkData, bool forceArrayUpdate)
{
  vtkUnsignedLongLongArray* temporalArrays = vtkUnsignedLongLongArray::SafeDownCast(vtkData->GetFieldData()->GetArray(vtkOmniConnectTemporalArrays::TemporalArraysFlagName));
  if (temporalArrays != nullptr)
  {
    omniGeomData.TimeVarying = (typename OmniConnectData::DataMemberId)temporalArrays->GetValue((int)OmniConnectData::GeomType);
    omniGeomData.UpdatesToPerform = omniGeomData.TimeVarying; // If not timevarying, arrays are not updated after first write (optimization). Use forceArrayUpdate to circumvent this behavior.
  }
  // If temporalArrays == nullptr, default .Timevarying constructor defines state (typically everything timevarying)

  if (forceArrayUpdate)
    omniGeomData.UpdatesToPerform = OmniConnectData::DataMemberId::ALL;
}

template<class T>
T* OmniConnectActorExistsAndInitialized(vtkOmniConnectActorNodeBase* aNode)
{
  T* actor = T::SafeDownCast(aNode->GetVtkProp3D());
  return aNode->ConnectorIsInitialized() && actor && actor->GetVisibility() ? actor : nullptr;
}

#endif