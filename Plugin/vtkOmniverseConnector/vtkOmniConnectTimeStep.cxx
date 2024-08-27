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

#include "vtkOmniConnectTimeStep.h"
#include "vtkOmniConnectVtkToOmni.h" 
#include "vtkOmniConnectLogCallback.h"
#include "vtkOmniConnectPassArrays.h"
#include "vtkOmniConnectTemporalArrays.h"
#include "OmniConnectUtilsExternal.h"
#include "vtkDataSet.h"
#include "vtkCompositeDataSet.h"
#include "vtkCompositeDataIterator.h"
#include "vtkCompositeDataDisplayAttributes.h"
#include "vtkPolyData.h"
#include "vtkFieldData.h"
#include "vtkPointData.h"
#include "vtkCellData.h"
#include "vtkMatrix4x4.h"
#include <cstring>

const char* vtkOmniConnectGenericPointArrayPrefix = "pv_point_";
const char* vtkOmniConnectGenericCellArrayPrefix = "pv_cell_";

namespace
{
  void UpdateArraysFromFieldData(vtkFieldData* fieldData, vtkOmniConnectTimeStep::DataEntry& dataEntry, bool cellArray, bool useNamePrefix, std::function< bool(vtkDataArray* arr)> AdmittedArrays)
  {
    const char* namePrefix = useNamePrefix ? 
      (cellArray ? vtkOmniConnectGenericCellArrayPrefix : vtkOmniConnectGenericPointArrayPrefix) :
      "";

    for (int i = 0; i < fieldData->GetNumberOfArrays(); ++i)
    {
      vtkDataArray* genArray = fieldData->GetArray(i);

      // First try to trivially retrieve the array
      auto arrayEntryIt = dataEntry.GenericArrays.begin();
      for (; arrayEntryIt != dataEntry.GenericArrays.end(); ++arrayEntryIt)
      {
        if (arrayEntryIt->second.DataArray == genArray)
          break;
      }

      if (arrayEntryIt == dataEntry.GenericArrays.end())
      {
        //Not trivially retrievable, so have to search by name
        if (AdmittedArrays(genArray))
        {
          std::string arrayName(namePrefix);
          arrayName += genArray->GetName();

          ocutils::FormatUsdName(const_cast<char*>(arrayName.data())); // Could pass library boundaries, so char*

          vtkOmniConnectTimeStep::ArrayEntry newEntry = { genArray, genArray->GetMTime(), cellArray, OmniConnectStatusType::UPDATE };
          auto itSuccessPair = dataEntry.GenericArrays.emplace(arrayName, newEntry);

          if (!itSuccessPair.second)
          {
            vtkOmniConnectTimeStep::ArrayEntry& arrayEntry = itSuccessPair.first->second; // The Entry-part (second) of the iterator (first) in the successPair
            arrayEntry.DataArray = genArray;
            arrayEntry.Status = (genArray->GetMTime() != arrayEntry.MTime) ? 
              OmniConnectStatusType::UPDATE : 
              OmniConnectStatusType::KEEP;
            arrayEntry.MTime = genArray->GetMTime();
          }
        }
      }
      else
      {
        //Optimized path that doesn't have to perform a name search
        vtkOmniConnectTimeStep::ArrayEntry& arrayEntry = arrayEntryIt->second;
        arrayEntry.Status = (genArray->GetMTime() != arrayEntry.MTime) ?
          OmniConnectStatusType::UPDATE :
          OmniConnectStatusType::KEEP;
        arrayEntry.MTime = genArray->GetMTime();
      }
    }
  }

  bool IsArrayTimeSeriesUpdated(vtkUnsignedLongLongArray* temporalArrays, const char* arrayName, bool cellArray)
  {
    const char* namePrefix = cellArray ? vtkOmniConnectGenericCellArrayPrefix : vtkOmniConnectGenericPointArrayPrefix;

    // The first NUM_GEOMTYPES values of the time series array denote limiting standard arrays (normals/texcoords etc.) of meshes, instancers, curves, volumes.
    for (int i = (int)OmniConnectGeomType::NUM_GEOMTYPES; i < temporalArrays->GetNumberOfValues(); ++i)
    {
      if (!strcmp((const char*)temporalArrays->GetValue(i), arrayName+strlen(namePrefix)))
        return true;
    }
    return false;
  }

}

void vtkOmniConnectTimeStep::ResetDataEntryCache()
{
  // Toggle all polyentries to be removed, from here on any polydata caches that should be kept have to be marked explicitly.
  for (auto& dataEntry : this->DataEntryMap)
  {
    dataEntry.second.Active = false;
  }
}

void vtkOmniConnectTimeStep::KeepDataEntryCache()
{
  // Keep polyentries
  for (auto& dataEntry : this->DataEntryMap)
  {
    dataEntry.second.Active = true;
  }
}

void vtkOmniConnectTimeStep::CleanupDataEntryCache()
{
  // push all removed polydatas into the delete queue and remove them from map
  for (auto it = this->DataEntryMap.begin(); it != this->DataEntryMap.end(); )
  {
    if (it->second.Active)
      ++it;
    else
      it = this->DataEntryMap.erase(it);
  }
}


bool vtkOmniConnectTimeStep::HasInputChanged(vtkDataObject* dataInput, vtkCompositeDataDisplayAttributes* cda)
{
  bool inputDataChanged = dataInput->GetMTime() > this->InputMTime;
  if (inputDataChanged)
    this->InputMTime = dataInput->GetMTime();

  if(cda && cda->GetMTime() > this->InputMTime)
  {
    this->InputMTime = cda->GetMTime();
    inputDataChanged = true;
  }

  vtkCompositeDataSet* comp = vtkCompositeDataSet::SafeDownCast(dataInput);
  if (comp)
  {
    vtkCompositeDataIterator* dit = comp->NewIterator();
    dit->SkipEmptyNodesOn();
    while (!dit->IsDoneWithTraversal())
    {
      vtkDataSet* subData = vtkDataSet::SafeDownCast(comp->GetDataSet(dit));
      if (subData && subData->GetMTime() > this->InputMTime)
      {
        this->InputMTime = subData->GetMTime();
        inputDataChanged = true;
      }
      dit->GoToNextItem();
    }
    dit->Delete();
  }

  return inputDataChanged;
}

bool vtkOmniConnectTimeStep::UpdateDataEntry(vtkDataSet* dataSet, size_t dataEntryId, bool forceUpdate, bool& forceArrayUpdate, int genericArrayFilter, bool useNamePrefix)
{
  // Effect of Temporal Arrays needs to be disabled on UpdatesToPerform and generic arrays, in case it itself changes
  vtkDataArray* temporalArrays = dataSet->GetFieldData()->GetArray(vtkOmniConnectTemporalArrays::TemporalArraysFlagName);
  vtkMTimeType fieldMTime = temporalArrays ? temporalArrays->GetMTime() : 0;
  forceArrayUpdate = false;

  DataEntry newEntry = { dataSet, dataSet->GetMTime(), fieldMTime, true };
  auto itSuccessPair = this->DataEntryMap.emplace(dataEntryId, newEntry);

  DataEntry& dataEntry = itSuccessPair.first->second;
  bool entryUpdated = itSuccessPair.second;
  if (!entryUpdated) // Entry was already present
  {
    forceArrayUpdate = dataEntry.FieldMTime != fieldMTime;
    dataEntry.FieldMTime = fieldMTime;

    dataEntry.Active = true;
    if (forceUpdate || dataEntry.DataSet != dataSet || dataEntry.MTime != dataSet->GetMTime()) // material/mapper changes require at least color/texture array updates.
    {
      entryUpdated = true;
      dataEntry.DataSet = dataSet;
      dataEntry.MTime = dataSet->GetMTime();
    }
  }

  if (entryUpdated)
  {
    // Set all generic arrays to Remove, then update the status as they appear in the pass arrays filter
    for (auto& entry : dataEntry.GenericArrays)
    {
      entry.second.Status = OmniConnectStatusType::REMOVE;
    }

    vtkDataArray* passArrays = dataSet->GetFieldData()->GetArray(vtkOmniConnectPassArrays::PassArraysFlagName);
    if (passArrays != nullptr)
    {
      this->UpdateGenericArrayCache(dataEntry, genericArrayFilter, useNamePrefix);
    }
  }

  return entryUpdated;
}

void vtkOmniConnectTimeStep::UpdateGenericArrayCache(DataEntry& dataEntry, int genericArrayFilter, bool useNamePrefix)
{
  assert(genericArrayFilter < 2);

  vtkPointData* pointData = dataEntry.DataSet->GetPointData();
  if (pointData && genericArrayFilter != 1)
  {
    vtkDataArray* normals = pointData->GetNormals();
    vtkDataArray* texcoords = pointData->GetTCoords();

    UpdateArraysFromFieldData(pointData, dataEntry, false, useNamePrefix,
      [&](vtkDataArray* arr)->bool
    {
      if (arr == normals || arr == texcoords)
        return false;
      vtkStdString arrayName = arr->GetName();
      return arrayName.compare("vtkOriginalPointIds") && arrayName.compare("vtkCompositeIndex");
    });
  }
  vtkCellData* cellData = dataEntry.DataSet->GetCellData();
  if (cellData && genericArrayFilter != 0)
  {
    UpdateArraysFromFieldData(cellData, dataEntry, true, useNamePrefix,
      [&](vtkDataArray* arr)->bool
    {
      vtkStdString arrayName = arr->GetName();
      return arrayName.compare("vtkOriginalCellIds") && arrayName.compare("vtkCompositeIndex"); //not equal to any of the automatically generated arrays
    });
  }
}

void vtkOmniConnectTimeStep::GetUpdatedDeletedGenericArrays(size_t dataEntryId, vtkOmniConnectGenericArrayList& updatedArrays, vtkOmniConnectGenericArrayList& deletedArrays, bool forceArrayUpdate)
{
  updatedArrays.resize(0);
  deletedArrays.resize(0);

  DataEntry& dataEntry = DataEntryMap.find(dataEntryId)->second;

  vtkUnsignedLongLongArray* temporalArrays = vtkUnsignedLongLongArray::SafeDownCast(dataEntry.DataSet->GetFieldData()->GetArray(vtkOmniConnectTemporalArrays::TemporalArraysFlagName));

  for (auto& x : dataEntry.GenericArrays)
  {
    ArrayEntry& arrayEntry = x.second;
    const char* arrayName = x.first.c_str();

    if ( arrayEntry.Status == OmniConnectStatusType::UPDATE ||
        (arrayEntry.Status == OmniConnectStatusType::KEEP && forceArrayUpdate) // Even though array is unchanged, its uniformity might have
       )
    {
      OmniConnectType dataType = GetOmniConnectType(arrayEntry.DataArray);

      if (dataType != OmniConnectType::UNDEFINED)
      {
        bool timeSeriesUpdated = temporalArrays ? IsArrayTimeSeriesUpdated(temporalArrays, arrayName, arrayEntry.CellArray) : true;
        bool isTimeVarying = timeSeriesUpdated; // Timevarying implies that the array has to be updated...
        if (forceArrayUpdate) // ...same with forceArrayUpdate
          timeSeriesUpdated = true;

        if (timeSeriesUpdated)
        {
          updatedArrays.emplace_back(
            arrayName,
            arrayEntry.CellArray,
            isTimeVarying,
            arrayEntry.DataArray->GetVoidPointer(0),
            arrayEntry.DataArray->GetNumberOfTuples(),
            dataType);
        }
      }
      else
      {
        std::string errorMsg("Array type not supported for ");
        errorMsg.append(x.first.c_str());
        vtkOmniConnectLogCallback::Callback(OmniConnectLogLevel::ERR, nullptr, errorMsg.c_str());
      }
    }
    else if (arrayEntry.Status == OmniConnectStatusType::REMOVE)
    {
      deletedArrays.emplace_back(
        arrayName,
        arrayEntry.CellArray,
        false,
        nullptr,
        0,
        OmniConnectType::UNDEFINED);
    }
  }
}

void vtkOmniConnectTimeStep::CleanupGenericArrayCache(size_t dataEntryId)
{
  auto omniDataMapIt = this->DataEntryMap.find(dataEntryId);
  DataEntry& dataEntry = omniDataMapIt->second;

  // delete all non-processed arrays
  for (auto it = dataEntry.GenericArrays.begin(); it != dataEntry.GenericArrays.end(); )
  {
    if (it->second.Status != OmniConnectStatusType::REMOVE)
    {
      ++it;
    }
    else
    {
      it = dataEntry.GenericArrays.erase(it);
    }
  }
}

bool vtkOmniConnectTimeStep::UpdateTransform(vtkMatrix4x4* mat)
{
  bool change = mat->GetMTime() > this->VtkTransformMTime;

  if (change)
    this->VtkTransformMTime = mat->GetMTime();

  return change;
}
