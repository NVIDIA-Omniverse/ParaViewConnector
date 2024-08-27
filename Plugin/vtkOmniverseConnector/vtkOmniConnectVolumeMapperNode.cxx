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

#include "vtkOmniConnectVolumeMapperNode.h"

#include "vtkOmniConnectVtkToOmni.h"
#include "vtkOmniConnectMapperNodeCommon.h"
#include "vtkOmniConnectVolumeNode.h"
#include "vtkOmniConnectRendererNode.h"
#include "vtkOmniConnectActorCache.h"
#include "vtkOmniConnectTextureCache.h"
#include "vtkOmniConnectTimeStep.h"
#include "vtkVolume.h"
#include "vtkFieldData.h"
#include "vtkColorTransferFunction.h"
#include "vtkPiecewiseFunction.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkObjectFactory.h"
#include "vtkVolume.h"
#include "vtkVolumeMapper.h"
#include "vtkVolumeProperty.h"
#include "vtkFloatArray.h"
#include "vtkPointData.h"
#include "vtkCellData.h"
#include "vtkUnsignedCharArray.h"
#include "vtkDataSetAttributes.h"

namespace
{
  void GatherTfData(const std::vector<float>& TfValues, const std::vector<float>& TfOValues, double* tfRange, OmniConnectTfData& tfData)
  {
    int numTfValues = (int)TfOValues.size();
    tfData.TfColors = &TfValues[0];
    tfData.TfColorsType = OmniConnectType::FLOAT3;
    tfData.TfOpacities = &TfOValues[0];
    tfData.TfOpacitiesType = OmniConnectType::FLOAT;
    tfData.TfNumValues = numTfValues;

    tfData.TfValueRange[0] = tfRange[0];
    tfData.TfValueRange[1] = tfRange[1];
  }

  void GatherMaterialData(OmniConnectMaterialData& omniMatData, size_t materialId, vtkDataArray* volArray, const std::vector<float>& TfValues, const std::vector<float>& TfOValues, double* tfRange)
  {
    omniMatData.MaterialId = materialId;
    omniMatData.VolumeMaterial = true;
    
    omniMatData.VolumeDataType = GetOmniConnectType(volArray);

    GatherTfData(TfValues, TfOValues, tfRange, omniMatData.TfData);
  }

  vtkIdType FindFirstGhostValue(vtkDataArray* volArray, vtkDataArray* ghostData, int ghostValue)
  {
    vtkIdType matchingIdx = -1;
    vtkUnsignedCharArray* ghostCharData = vtkUnsignedCharArray::SafeDownCast(ghostData);

    vtkIdType numValues = volArray->GetNumberOfTuples();

    if(ghostCharData && (numValues == ghostCharData->GetNumberOfTuples()))
    {
      for(vtkIdType i = 0; i < numValues && matchingIdx == -1; ++i)
      {
        if(ghostCharData->GetValue(i) == ghostValue)
          matchingIdx = i;
      }
    }

    return matchingIdx;
  }

  void GatherVolumeData(OmniConnectVolumeData& omniVolumeData, vtkImageData* vtkVolData, vtkDataArray* volArray, vtkFloatArray* flattenedArray, vtkVolumeProperty* volProperty, int cellFlag,
    const std::vector<float>& TfValues, const std::vector<float>& TfOValues, double* tfRange)
  {
    // Find the background value
    vtkIdType backgroundIdx = -1;
    {
      if(cellFlag == 0)
      {
        vtkDataArray* pointGhostData = vtkVolData->GetPointData()->GetArray(vtkDataSetAttributes::GhostArrayName());
        if(pointGhostData)
          backgroundIdx = FindFirstGhostValue(volArray, pointGhostData, vtkDataSetAttributes::HIDDENPOINT);
      }
      else
      {
        vtkDataArray* cellGhostData = vtkVolData->GetCellData()->GetArray(vtkDataSetAttributes::GhostArrayName());
        if(cellGhostData)
          backgroundIdx = FindFirstGhostValue(volArray, cellGhostData, vtkDataSetAttributes::HIDDENCELL);
      }
    }

    // Deal with the case of more than one data component (for preclassified volumes). This part is effectively a pre-processed transfer function, 
    // so it makes the Geom data dependent on material structures, hence a requirement for GetMaterialsChanged().
    int ncomp = volArray->GetNumberOfComponents();
    if (ncomp > 1)
    {
      int vectorComp = volProperty->GetRGBTransferFunction()->GetVectorComponent();

      flattenedArray->SetNumberOfComponents(1);
      flattenedArray->SetNumberOfTuples(volArray->GetNumberOfTuples());
      if (volProperty->GetRGBTransferFunction()->GetVectorMode() !=
        vtkColorTransferFunction::MAGNITUDE)
      {
        flattenedArray->CopyComponent(0, volArray, vectorComp);
      }
      else
      {
        for (vtkIdType t = 0; t < volArray->GetNumberOfTuples(); ++t)
        {
          flattenedArray->SetTuple1(t, vtkMath::Norm(volArray->GetTuple3(t)));
        }
      }
      volArray = flattenedArray;
    }

    // Set the volume data from volArray
    omniVolumeData.Data = volArray->GetVoidPointer(0);
    omniVolumeData.DataType = GetOmniConnectType(volArray);

    // Get the spatial information from vtkVolData and set (does not have to be transformed with actor matrix, as this is implicit in the usd hierarchy)
    int dims[3];
    vtkVolData->GetDimensions(dims);
    double spacing[3];
    vtkVolData->GetSpacing(spacing);
    const double* bds = vtkVolData->GetBounds();
    
    int dd = (cellFlag == 1) ? 1 : 0;
    omniVolumeData.NumElements[0] = dims[0] - dd;
    omniVolumeData.NumElements[1] = dims[1] - dd;
    omniVolumeData.NumElements[2] = dims[2] - dd;

    omniVolumeData.Origin[0] = bds[0];
    omniVolumeData.Origin[1] = bds[2];
    omniVolumeData.Origin[2] = bds[4];

    omniVolumeData.CellDimensions[0] = spacing[0];
    omniVolumeData.CellDimensions[1] = spacing[1];
    omniVolumeData.CellDimensions[2] = spacing[2];

    omniVolumeData.BackgroundIdx = backgroundIdx;
    
    // Transferfunction data (in case it becomes part of the volume data output itself)
    GatherTfData(TfValues, TfOValues, tfRange, omniVolumeData.TfData);
  }
}

//============================================================================
class vtkOmniConnectVolumeMapperNodeInternals
{
public:

  vtkOmniConnectVolumeMapperNodeInternals()
  {
    FlattenedArray = vtkFloatArray::New();
  }

  ~vtkOmniConnectVolumeMapperNodeInternals()
  {
    FlattenedArray->Delete();
  }

  std::vector<size_t> MaterialIds;
  bool ScalarModeChanged = false;
  bool VectorModeChanged = false;
  bool VectorComponentChanged = false;
  int LastScalarMode = -1;
  int LastVectorMode = -1;
  int LastVectorComponent = -1;
  vtkFloatArray* FlattenedArray = nullptr;
};

//============================================================================
vtkStandardNewMacro(vtkOmniConnectVolumeMapperNode);

//------------------------------------------------------------------------------
vtkOmniConnectVolumeMapperNode::vtkOmniConnectVolumeMapperNode()
  : Internals(new vtkOmniConnectVolumeMapperNodeInternals())
  , TfValues(vtkOmniConnectVolumeMapperNode::NumTfValues*3)
  , TfOValues(vtkOmniConnectVolumeMapperNode::NumTfValues)
{
}

//------------------------------------------------------------------------------
vtkOmniConnectVolumeMapperNode::~vtkOmniConnectVolumeMapperNode()
{
  delete this->Internals;
  this->Internals = nullptr;
}

//------------------------------------------------------------------------------
void vtkOmniConnectVolumeMapperNode::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//------------------------------------------------------------------------------
void vtkOmniConnectVolumeMapperNode::Build(bool prepass)
{
}

//----------------------------------------------------------------------------
void vtkOmniConnectVolumeMapperNode::ResetMaterialIds()
{
  // Reset the visited material temp array
  this->Internals->MaterialIds.resize(0);
}

//----------------------------------------------------------------------------
void vtkOmniConnectVolumeMapperNode::UpdateVolSpecificChanges(vtkVolumeMapper* mapper, vtkVolumeProperty* volProperty)
{
  if (mapper->GetScalarMode() != this->Internals->LastScalarMode)
  {
    this->Internals->LastScalarMode = mapper->GetScalarMode();

    this->Internals->ScalarModeChanged = true;
  }
  else
  {
    this->Internals->ScalarModeChanged = false;
  }

  int vectorMode = volProperty->GetRGBTransferFunction()->GetVectorMode();
  if (vectorMode != this->Internals->LastVectorMode)
  {
    this->Internals->LastVectorMode = vectorMode;

    this->Internals->VectorModeChanged = true;
  }
  else
  {
    this->Internals->VectorModeChanged = false;
  }

  int vectorComp = volProperty->GetRGBTransferFunction()->GetVectorComponent();
  if(vectorComp != this->Internals->LastVectorComponent)
  {
    this->Internals->LastVectorComponent = vectorComp;

    this->Internals->VectorComponentChanged = true;
  }
  else
  {
    this->Internals->VectorComponentChanged = false;
  }
}


//----------------------------------------------------------------------------
void vtkOmniConnectVolumeMapperNode::RenderVolume(vtkOmniConnectActorNodeBase* aNode, vtkVolume* actor, vtkVolumeMapper* mapper, vtkImageData* volData, size_t dataEntryId, size_t materialId)
{
  // Some standard stuff
  vtkOmniConnectRendererNode* rNode = vtkOmniConnectRendererNode::GetRendererNode(this);
  OmniConnect* connector = rNode->GetOmniConnector();
  size_t actorId = aNode->GetActorId();
  size_t volGeomId = dataEntryId; // Usd geomId equivalent to vtk dataEntryId
  double animTimeStep = actor->GetPropertyKeys() ? actor->GetPropertyKeys()->Get(vtkDataObject::DATA_TIME_STEP()) : 0.0f;
  vtkOmniConnectTimeStep* omniTimeStep = aNode->GetTimeStep(animTimeStep);
  bool scalarModeChanged = this->Internals->ScalarModeChanged;
  bool vectorModeChanged = this->Internals->VectorModeChanged;
  bool vectorComponentChanged = this->Internals->VectorComponentChanged;

  int cellFlag; // 0: per-points, 1: per-cell, 2: generic field arrays
  vtkDataArray* volArray = vtkDataArray::SafeDownCast(this->GetArrayToProcess(volData, cellFlag));
  if (!volArray || cellFlag >= 2)
    return;

  double* volRange = volArray->GetRange();
  vtkVolumeProperty* volProperty = actor->GetProperty(); // Required for GatherVolumeData() as well

  //
  // Create/update material entry in cache and send to connector
  //
  if (aNode->GetMaterialsChanged())
  {
    UpdateTransferFunctions(volProperty, volRange);

    aNode->SetSubGeomProgress(0.1);

    bool materialProcessed = false;
    if (!materialProcessed)
    {
      // Update Material

      // This is the first time we encounter this material within this particular renderpass, so update the definition on the OmniConnect side.
      {
        OmniConnectMaterialData omniMatData;
        SetUpdatesToPerform(omniMatData, rNode->GetForceTimeVaryingMaterialParams());

        GatherMaterialData(omniMatData, materialId, volArray, this->TfValues, this->TfOValues, this->TfRange);

        connector->UpdateMaterial(actorId, omniMatData, animTimeStep);
        this->Internals->MaterialIds.push_back(materialId);
      }
      // Materials are not deleted because they are not stored per-timestep and they have no means of identification across timesteps; as such they are simply overwritten by all new materials generated within this timestep update. 
      // If a previous timestep update for this actor used more materials, the ones with higher indices simply remain and do no harm (with the added benefit that older timesteps can still be displayed).
    }
  }

  aNode->SetSubGeomProgress(0.2);

  //
  // Create/update material entry in cache and send to connector
  //
  // Add usd geom entry (geomId)
  bool newVolGeom = aNode->AddTransferredGeometry(volGeomId, OmniConnectGeomType::VOLUME); 

  // Make sure the vtk cache entry is kept alive
  bool forceVolDataUpdate = aNode->GetMaterialsChanged() || vectorModeChanged || vectorComponentChanged || scalarModeChanged; // MaterialsChanged() can be removed once tf is separated out of volData update
  bool forceArrayUpdate = false;
  bool updateVolData = omniTimeStep->UpdateDataEntry(volData, volGeomId, forceVolDataUpdate, forceArrayUpdate, cellFlag, false); // Only add arrays which correspond in celltype to volArray

  // Update double to float conversion setting specifically for the dataset currently processed
  SetConvertGenericArraysDoubleToFloat(connector, volData, true);

  if (updateVolData)
  {
    vtkOmniConnectTempArrays& tempArrays = rNode->GetTempArrays();

    forceArrayUpdate = true; // Always update, since the generic arrays are written into the VDBs, which are always completely rewritten anyway
      //forceArrayUpdate || newVolGeom || scalarModeChanged;

    // Extract updated and deleted generic arrays from vtk
    omniTimeStep->GetUpdatedDeletedGenericArrays(dataEntryId,
      tempArrays.UpdatedGenericArrays,
      tempArrays.DeletedGenericArrays,
      forceArrayUpdate);

    aNode->SetSubGeomProgress(0.4);

    size_t ugaLen = tempArrays.UpdatedGenericArrays.size();
    size_t dgaLen = tempArrays.DeletedGenericArrays.size();
    OmniConnectGenericArray* updatedGenericArrays = (ugaLen > 0 ? &tempArrays.UpdatedGenericArrays[0] : nullptr);
    OmniConnectGenericArray* deletedGenericArrays = (dgaLen > 0 ? &tempArrays.DeletedGenericArrays[0] : nullptr);

    {
      OmniConnectVolumeData omniVolumeData;
      omniVolumeData.VolumeId = volGeomId;
      omniVolumeData.preClassified = rNode->GetMergeTfIntoVol();
      SetUpdatesToPerform(omniVolumeData, volData, forceArrayUpdate); // Fill out omniVolumeData.UpdatesToPerform: which standard arrays of the OmniConnectVolumeData type to perform updates on (for manual disabling of standard array updates over timesteps).

      GatherVolumeData(omniVolumeData, volData, volArray, Internals->FlattenedArray, volProperty, cellFlag, this->TfValues, this->TfOValues, this->TfRange);

      connector->UpdateVolume(actorId, animTimeStep, omniVolumeData, materialId, 
        updatedGenericArrays, ugaLen, deletedGenericArrays, dgaLen);
      connector->SetGeomVisibility(actorId, omniVolumeData.VolumeId, OmniConnectGeomType::VOLUME, true, animTimeStep); //Always set geom to visible (geometry that became invisible handled in vtkOmniConnectActorNodeBase)
    }
    
    aNode->SetSubGeomProgress(0.9);

    // Cleanup generic array cache (to match arrays that were deleted in call to connector->UpdateMesh())
    omniTimeStep->CleanupGenericArrayCache(dataEntryId);
  }
}

void vtkOmniConnectVolumeMapperNode::UpdateTransferFunctions(vtkVolumeProperty* volProperty, double* volRange)
{
  vtkColorTransferFunction* colorTF = volProperty->GetRGBTransferFunction(0);
  vtkPiecewiseFunction* scalarTF = volProperty->GetScalarOpacity(0);

  colorTF->GetRange(this->TfRange);
  // Alternative for when tfRange is invalid
  if (volRange && (volRange[1] > volRange[0]) && (this->TfRange[1] <= this->TfRange[0]))
  {
    this->TfRange[0] = volRange[0];
    this->TfRange[1] = volRange[1];
  }

  scalarTF->GetTable(this->TfRange[0], this->TfRange[1], vtkOmniConnectVolumeMapperNode::NumTfValues, &this->TfOValues[0]);
  colorTF->GetTable(this->TfRange[0], this->TfRange[1], vtkOmniConnectVolumeMapperNode::NumTfValues, &this->TfValues[0]);
}

//------------------------------------------------------------------------------
void vtkOmniConnectVolumeMapperNode::Render(bool prepass)
{
  if (prepass)
  {
    // we use a lot of params from our parent
    vtkOmniConnectActorNodeBase* aNode = &(vtkOmniConnectVolumeNode::SafeDownCast(this->Parent)->ActorNodeBase);
    vtkVolume* actor = nullptr;

    // Guard against uninitialized actors or initialized ones that are invisible.
    if (!(actor = OmniConnectActorExistsAndInitialized<vtkVolume>(aNode)))
      return;

    vtkVolumeMapper* mapper = vtkVolumeMapper::SafeDownCast(this->GetRenderable());
    vtkOmniConnectRendererNode* rNode = vtkOmniConnectRendererNode::GetRendererNode(this);
    OmniConnect* connector = rNode->GetOmniConnector();

    //Not sure if the following is required, see ray tracing backend:
    mapper->GetInputAlgorithm()->UpdateInformation();
    mapper->GetInputAlgorithm()->Update();
    //~

    // Get the timestep
    double animTimeStep = actor->GetPropertyKeys() ? actor->GetPropertyKeys()->Get(vtkDataObject::DATA_TIME_STEP()) : 0.0f;
    vtkOmniConnectTimeStep* omniTimeStep = aNode->GetTimeStep(animTimeStep);

    // Prepare vtk dataentry cache for node rebuild (set to remove unless explicitly defined otherwise).
    omniTimeStep->ResetDataEntryCache();
    this->ResetMaterialIds();
    this->UpdateVolSpecificChanges(mapper, actor->GetProperty());

    //Check whether any input imagedatas changed
    vtkImageData* volData = vtkImageData::SafeDownCast(mapper->GetDataSetInput());

    if (volData)
    {
      aNode->SetSubGeomProgress(0.0);

      // Find out whether and (sub)images have changed
      bool inputDataChanged = omniTimeStep->HasInputChanged(volData);
      bool processNode = aNode->GetMaterialsChanged() || inputDataChanged;
      if (processNode)
      {
        RenderVolume(aNode, actor, mapper, volData, 0, 0);
      }
      else
      {
        // Keep all vtk dataentry caches (dataEntryId)
        omniTimeStep->KeepDataEntryCache();
        // Make sure no earlier usd geometries are thrown out either (geomId)
        aNode->KeepTransferredGeometries();
      }

      aNode->SetSubGeomProgress(0.99);
    }

    // Clean up vtk dataentry cache 
    omniTimeStep->CleanupDataEntryCache();
  }
}
