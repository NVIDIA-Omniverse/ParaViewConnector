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

#ifndef vtkOmniConnectTimeStep_h
#define vtkOmniConnectTimeStep_h

#include "OmniConnect.h"
#include "vtkUnsignedLongLongArray.h"
#include "vtkOmniConnectVtkToOmni.h"

#include <vector>
#include <map>

class vtkDataObject;
class vtkDataSet;
class vtkMatrix4x4;
class vtkCompositeDataDisplayAttributes;
struct OmniConnectGenericArray;

typedef std::vector<OmniConnectGenericArray> vtkOmniConnectGenericArrayList;

extern const char* vtkOmniConnectGenericPointArrayPrefix;
extern const char* vtkOmniConnectGenericCellArrayPrefix;

struct vtkOmniConnectTempArrays
{
  std::vector<unsigned int> IndexArray;
  std::vector<unsigned int> IndexToCell;
  std::vector<int> VertexToCell;
  std::vector<float> PerPrimNormal;
  std::vector<unsigned char> PerPrimColor;
  std::vector<int> CurveLengths;
  std::vector<float> PointsArray;
  std::vector<float> TexCoordsArray;
  std::vector<unsigned char> ColorsArray;
  std::vector<float> ScalesArray;
  std::vector<float> OrientationsArray;
  std::vector<int64_t> InvisibleIndicesArray;
  std::vector<std::vector<char>> GenericArrays;
  vtkOmniConnectGenericArrayList UpdatedGenericArrays;
  vtkOmniConnectGenericArrayList DeletedGenericArrays;

  size_t GetNumGenericArrays() const { return UpdatedGenericArrays.size(); }

  bool HasPerCellGenericArrays() const 
  {
    for(int i = 0; i < GetNumGenericArrays(); ++i)
    {
      if(UpdatedGenericArrays[i].PerPoly)
        return true;
    }
    return false;
  }

  size_t SyncNumGenericArrays()
  {
    size_t ugaLen = GetNumGenericArrays();
    if(ugaLen > GenericArrays.size())
      GenericArrays.resize(ugaLen);
    return ugaLen;
  }

  void ResetGenericArray(size_t arrayIdx, size_t numElements)
  {
    assert(arrayIdx < UpdatedGenericArrays.size());
    if(UpdatedGenericArrays[arrayIdx].Data)
    {
      size_t eltSize = GetOmniConnectTypeSize(UpdatedGenericArrays[arrayIdx].DataType);
      GenericArrays[arrayIdx].resize(numElements*eltSize);
    }
    else
      GenericArrays[arrayIdx].resize(0);
  }
  
  void ReserveGenericArray(size_t arrayIdx, size_t numElements)
  {
    assert(arrayIdx < UpdatedGenericArrays.size());
    if(UpdatedGenericArrays[arrayIdx].Data)
    {
      size_t eltSize = GetOmniConnectTypeSize(UpdatedGenericArrays[arrayIdx].DataType);
      GenericArrays[arrayIdx].reserve(numElements*eltSize);
    } 
  }

  size_t ExpandGenericArray(size_t arrayIdx, size_t numElements)
  {
    assert(arrayIdx < UpdatedGenericArrays.size());
    if(UpdatedGenericArrays[arrayIdx].Data)
    {
      size_t eltSize = GetOmniConnectTypeSize(UpdatedGenericArrays[arrayIdx].DataType);
      size_t startByte = GenericArrays[arrayIdx].size();
      GenericArrays[arrayIdx].resize(startByte+numElements*eltSize);
      return (eltSize != 0) ? startByte/eltSize : 0;
    }
    return 0;
  }

  void CopyToGenericArray(size_t arrayIdx, size_t srcIdx, size_t destIdx, size_t numElements)
  {
    assert(arrayIdx < UpdatedGenericArrays.size());
    if(UpdatedGenericArrays[arrayIdx].Data)
    {
      size_t eltSize = GetOmniConnectTypeSize(UpdatedGenericArrays[arrayIdx].DataType);
      assert(srcIdx+numElements <= UpdatedGenericArrays[arrayIdx].NumElements);
      const void* attribSrc = reinterpret_cast<const char*>(UpdatedGenericArrays[arrayIdx].Data) + srcIdx*eltSize;
      size_t dstStart = destIdx*eltSize;
      size_t numBytes = numElements*eltSize;
      assert(dstStart+numBytes <= GenericArrays[arrayIdx].size());
      void* attribDest = &GenericArrays[arrayIdx][dstStart];
      memcpy(attribDest, attribSrc, numBytes);
    }
  }

  void AssignGenericArraysToUpdated(bool convertToVertexLayout)
  {
    for(int i = 0; i < GetNumGenericArrays(); ++i)
    {
      size_t eltSize = GetOmniConnectTypeSize(UpdatedGenericArrays[i].DataType);
      UpdatedGenericArrays[i].Data = GenericArrays[i].size() ? GenericArrays[i].data() : nullptr;
      UpdatedGenericArrays[i].NumElements = (eltSize != 0) ? (GenericArrays[i].size() / eltSize) : 0;
      if (convertToVertexLayout)
        UpdatedGenericArrays[i].PerPoly = false;
      
      size_t sizeCheck = UpdatedGenericArrays[i].PerPoly ? CurveLengths.size() : PointsArray.size() / 3;
      assert(sizeCheck == UpdatedGenericArrays[i].NumElements);
    }
  }
};

struct vtkOmniConnectTimeStep
{
  struct ArrayEntry
  {
    vtkDataArray* DataArray = nullptr; 
    vtkMTimeType MTime = 0;
    bool CellArray = false;
    OmniConnectStatusType Status; 
  };

  struct DataEntry
  {
    vtkDataSet* DataSet = nullptr; // vtkPolyData, vtkImageData
    vtkMTimeType MTime = 0;
    vtkMTimeType FieldMTime = 0;
    bool Active = true;
    std::map<std::string, ArrayEntry> GenericArrays;
  };
 
  void ResetDataEntryCache();
  void KeepDataEntryCache();
  void CleanupDataEntryCache();

  bool HasInputChanged(vtkDataObject* dataInput, vtkCompositeDataDisplayAttributes* cda = nullptr); // Shortcut that evaluates whether any of the data entries or their composition could have changed
  bool UpdateDataEntry(vtkDataSet* dataSet, size_t dataEntryId, bool forceUpdate, bool& forceArrayUpdate, int genericArrayFilter = -1, bool useNamePrefix = true); // genericArrayFilter: 0==points, 1==cells, -1==all

  void UpdateGenericArrayCache(DataEntry& dataEntry, int genericArrayFilter, bool useNamePrefix);
  void GetUpdatedDeletedGenericArrays(size_t dataEntryId, vtkOmniConnectGenericArrayList& updatedArrays, vtkOmniConnectGenericArrayList& deletedArrays, bool forceArrayUpdate);
  void CleanupGenericArrayCache(size_t dataEntryId);

  bool UpdateTransform(vtkMatrix4x4* mat);

  size_t GetNumDataEntries() const { return DataEntryMap.size(); }

  std::map< size_t, DataEntry > DataEntryMap; // Detect per-dataset/per array changes
  vtkMTimeType InputMTime = 0; // MTime over all data input (datasets) for the timestep together, including changes to the set/tree of datasets itself 
  vtkMTimeType VtkTransformMTime = 0; // Detect a change in transforms
  // ..material-related data
};


#endif
