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

#include "vtkOmniConnectActorNodeBase.h"
#include "vtkOmniConnectRendererNode.h"
#include "OmniConnect.h"
#include "vtkOmniConnectActorCache.h"
#include "vtkOmniConnectTextureCache.h"
#include "vtkInformation.h"
#include "vtkActor.h"
#include "vtkVolume.h"
#include "vtkMapper.h"
#include "vtkCompositeDataIterator.h"
#include "vtkCompositeDataSet.h"
#include "vtkSmartVolumeMapper.h"
#include "vtkMultiBlockVolumeMapper.h"
#include "vtkPolyData.h"
#include "vtkProperty.h"
#include "vtkTexture.h"
#include "vtkImageData.h"
#include "vtkMatrix4x4.h"
#include "vtkDataObjectTree.h"

#define SUB_GEOM_PROGRESS_WEIGHTING 0.5


//============================================================================
namespace
{
  bool PolyDataHasCells(vtkPolyData* polyData)
  {
    vtkCellArray* prims[] = {
          polyData->GetVerts(),
          polyData->GetLines(),
          polyData->GetPolys(),
          polyData->GetStrips()
    };

    bool hasCells =
      (prims[0] && prims[0]->GetNumberOfCells()) ||
      (prims[1] && prims[1]->GetNumberOfCells()) ||
      (prims[2] && prims[2]->GetNumberOfCells()) ||
      (prims[3] && prims[3]->GetNumberOfCells());

    return hasCells;
  }
}

//============================================================================
class vtkOmniConnectActorNodeBaseInternals
{
public:
  vtkOmniConnectActorNodeBaseInternals() {}
  ~vtkOmniConnectActorNodeBaseInternals() { Reset(); }

  bool IsInitialized() const { return ConnectActorCache != nullptr; }
  void Initialize(vtkProp3D* prop3D, size_t actorId)
  {
    ConnectActorCache = new vtkOmniConnectActorCache(prop3D, actorId);
    ConnectActorCache->Initialize();
  }

  void Reset()
  {
    delete ConnectActorCache;
    ConnectActorCache = nullptr;
  }

  void ReportProgress(vtkOmniConnectRendererNode* rNode, bool showInGui)
  {
    double fullActorPercentage = (1- SUB_GEOM_PROGRESS_WEIGHTING) * this->ActorProgress + this->SubGeomProgress * SUB_GEOM_PROGRESS_WEIGHTING;
    rNode->SetActorProgress(fullActorPercentage, showInGui);
  }

  void SetActorProgress(vtkOmniConnectRendererNode* rNode, double actorPercentage)
  {
    if (actorPercentage == 0.0)
      this->SubGeomProgress = 0.0;

    this->ActorProgress = actorPercentage;
    ReportProgress(rNode, false);
  }

  void SetSubGeomProgress(vtkOmniConnectRendererNode* rNode, double subGeomPercentage)
  {
    this->SubGeomProgress = subGeomPercentage;
    ReportProgress(rNode, true);
  }

  bool HasActorName(vtkProp3D* prop3D);
  void CountNumberOfSubGeoms(vtkProp3D* prop3D);

  vtkOmniConnectActorCache* ConnectActorCache = nullptr;
  int NumberOfSubGeoms = 0;
  int NumSubGeomsProcessed = -1;
  double ActorProgress = 0.0;
  double SubGeomProgress = 0.0;
};

//----------------------------------------------------------------------------
bool vtkOmniConnectActorNodeBaseInternals::HasActorName(vtkProp3D* prop3D)
{
  vtkInformation* info = prop3D->GetPropertyKeys();
  return info && info->Has(vtkOmniConnectRendererNode::ACTORNAME());
}

//----------------------------------------------------------------------------
void vtkOmniConnectActorNodeBaseInternals::CountNumberOfSubGeoms(vtkProp3D* prop3D)
{
  this->NumberOfSubGeoms = 0;
  this->NumSubGeomsProcessed = -1;

  if (vtkActor* actor = vtkActor::SafeDownCast(prop3D))
  {
    vtkMapper* mapper = actor->GetMapper();
    vtkPolyData* polyData = nullptr;
    vtkDataObject* dobj = nullptr;
    if (mapper->GetNumberOfInputPorts() > 0)
    {
      dobj = mapper->GetInputDataObject(0, 0);
      polyData = vtkPolyData::SafeDownCast(dobj);
    }

    if (polyData)
    {
      bool hasCells = PolyDataHasCells(polyData);

      if (hasCells)
        this->NumberOfSubGeoms = 1;
    }
    else if (dobj)
    {
      bool hasCells = false;
      vtkCompositeDataSet* comp = vtkCompositeDataSet::SafeDownCast(dobj);
      if (comp)
      {
        vtkCompositeDataIterator* dit = comp->NewIterator();
        dit->SkipEmptyNodesOn();
        while (!dit->IsDoneWithTraversal())
        {
          polyData = vtkPolyData::SafeDownCast(comp->GetDataSet(dit));
          if (polyData)
          {
            hasCells = hasCells || PolyDataHasCells(polyData);
            this->NumberOfSubGeoms++;
          }

          dit->GoToNextItem();
        }
        dit->Delete();
      }
      if (!hasCells)// If there is no content at all, ignore all subgeoms
        this->NumberOfSubGeoms = 0;
    }
  }
  else if (vtkVolume* volume = vtkVolume::SafeDownCast(prop3D))
  {
    if (vtkSmartVolumeMapper::SafeDownCast(volume->GetMapper()))
    {
      this->NumberOfSubGeoms = 1;
    }
    else if (vtkMultiBlockVolumeMapper* mapper = vtkMultiBlockVolumeMapper::SafeDownCast(volume->GetMapper()))
    {
      // Ideally we'd just force-load the submappers and count those
      vtkObject* input = mapper->GetDataSetInput();
      if (vtkDataObjectTree* inputTree = vtkDataObjectTree::SafeDownCast(input))
      {
        // Not handled yet, can iterate over the dataobjecttree if no mappers available
      }
      else if (vtkImageData* currentIm = vtkImageData::SafeDownCast(input))
      {
        this->NumberOfSubGeoms = 1;
      }
    }
  }
}


//----------------------------------------------------------------------------
vtkOmniConnectActorNodeBase::vtkOmniConnectActorNodeBase()
  : Internals(new vtkOmniConnectActorNodeBaseInternals)
{
}

//----------------------------------------------------------------------------
vtkOmniConnectActorNodeBase::~vtkOmniConnectActorNodeBase() 
{
  // Only delete if the renderernode itself is not being removed.
  if (!this->InRendererDestructor && ConnectorIsInitialized())
  {
    this->DeleteActor();
  }

  delete this->Internals;
  this->Internals = nullptr;
}

//----------------------------------------------------------------------------
bool vtkOmniConnectActorNodeBase::HasConnectorContent() const
{
  return ConnectorIsInitialized();
}

//----------------------------------------------------------------------------
void vtkOmniConnectActorNodeBase::KeepTransferredGeometries()
{
  this->Internals->ConnectActorCache->KeepTransferredGeometries();
}

//----------------------------------------------------------------------------
bool vtkOmniConnectActorNodeBase::AddTransferredGeometry(size_t geomId, OmniConnectGeomType geomType)
{
  return this->Internals->ConnectActorCache->AddTransferredGeometry(geomId, geomType);
}

//----------------------------------------------------------------------------
vtkOmniConnectTextureCache* vtkOmniConnectActorNodeBase::UpdateTextureCache(vtkImageData* texData, size_t materialId, bool isColorTextureMap)
{
  return this->Internals->ConnectActorCache->UpdateTextureCache(texData, materialId, isColorTextureMap);
}

//----------------------------------------------------------------------------
size_t vtkOmniConnectActorNodeBase::GetActorId()
{
  return this->Internals->ConnectActorCache->GetId();
}

//----------------------------------------------------------------------------
vtkOmniConnectTimeStep* vtkOmniConnectActorNodeBase::GetTimeStep(double animTimeStep)
{
  return this->Internals->ConnectActorCache->GetTimeStep(animTimeStep);
}

//----------------------------------------------------------------------------
void vtkOmniConnectActorNodeBase::ConnectorBuild(bool prepass, vtkOmniConnectRendererNode* rendererNode)
{
  if (prepass)
  {
    if (ConnectorIsInitialized())
    {
      if(Internals->ConnectActorCache->NameHasChanged())
        this->DeleteActor(); // remove the actor (connector cache has already been reset)
    }

    bool noWidgetOrAllowed = rendererNode->GetAllowWidgetActors() || Internals->HasActorName(this->VtkProp3D);

    this->Internals->CountNumberOfSubGeoms(this->VtkProp3D); // This needs to be called to determine hasConnectorContent and progress
    bool hasConnectorContent = this->Internals->NumberOfSubGeoms > 0;

    // Only create a connector entry if the actor is visible and there is content for it
    // (Once initialized, it will not be removed if made invisible)
    if (!ConnectorIsInitialized() && this->VtkProp3D->GetVisibility() && noWidgetOrAllowed && hasConnectorContent)
    {
      this->RendererNode = rendererNode;
      assert(this->RendererNode);
      size_t actorId = this->RendererNode->NewActorId();

      //Initialize the actor cache
      Internals->Initialize(this->VtkProp3D, actorId);

      OmniConnect* connector = this->RendererNode->GetOmniConnector();

      //Create the actor in Omniverse Connector
      connector->CreateActor(actorId, Internals->ConnectActorCache->GetActorName().c_str());

      //In case we reopened an existing session, the scene to anim times have to be restored on the actor cache
      std::vector<double>& sceneToAnimTimes = Internals->ConnectActorCache->GetSceneToAnimationTimes();
      if(sceneToAnimTimes.size() == 0 && !connector->GetSettings().CreateNewOmniSession)
      {
        size_t numSceneToAnimTimes = connector->GetNumSceneToAnimTimes(actorId);
        if(numSceneToAnimTimes > 0)
        {
          sceneToAnimTimes.resize(numSceneToAnimTimes*2);
          connector->RestoreSceneToAnimTimes(sceneToAnimTimes.data(), numSceneToAnimTimes);
        }
      }
    }
    else if(ConnectorIsInitialized() && !noWidgetOrAllowed)
    {
      this->DeleteActor();
    }
  }
}

//----------------------------------------------------------------------------
void vtkOmniConnectActorNodeBase::ConnectorRender(bool prepass)
{
  if (!ConnectorIsInitialized())
    return;

  bool actVisible = this->VtkProp3D->GetVisibility();
  vtkOmniConnectRendererNode* rNode = this->RendererNode;
  OmniConnect* connector = rNode->GetOmniConnector();
  vtkOmniConnectActorCache* connectActorCache = this->Internals->ConnectActorCache;
  
  size_t actorId = connectActorCache->GetId();

  double sceneTimeStep = rNode->GetSceneTime();
  double animTimeStep = this->VtkProp3D->GetPropertyKeys() ? this->VtkProp3D->GetPropertyKeys()->Get(vtkDataObject::DATA_TIME_STEP()) : 0.0f;

  // Store changes in a persistent place, as this routine is split in execution between prepass and postpass
  if (prepass)
  {
    this->VisibilityChanged = connectActorCache->UpdateVisibilityChanges();
    this->TransformChanged = false;
    this->TextureChanged = false;
    this->GeometryChanged = false;

    //Visibility changes
    if (this->VisibilityChanged)
    {
      connector->SetActorVisibility(actorId, actVisible);

      if (!actVisible) // Flush updates, as no updates will be performed for invisible actors.
      {
        connector->FlushActorUpdates(actorId);
        rNode->FlushSceneUpdates();
      }
    }

    if (actVisible)
    {
      rNode->SetProgressText(connectActorCache->GetActorName().c_str());
      this->Internals->SetActorProgress(rNode, 0.0);

      //
      // Actor changes
      //

      // Timestep range
      vtkOmniConnectTimeStep* omniTimeStep = connectActorCache->GetTimeStep(animTimeStep);
      connector->AddTimeStep(actorId, animTimeStep);

      this->Internals->SetActorProgress(rNode, 0.1);

      // Transform changes
      this->TransformChanged = connectActorCache->UpdateTransformChanges(omniTimeStep);
      if (this->TransformChanged)
      {
        connector->SetActorTransform(actorId, animTimeStep, this->VtkProp3D->GetMatrix()->GetData());
      }

      this->Internals->SetActorProgress(rNode, 0.2);

      // Actor and material change info for inputdatas
      this->ActorChanged = connectActorCache->UpdateActorChanges();
      this->MaterialsChanged = connectActorCache->UpdateMaterialChanges() || rNode->GetMaterialPolicyChanged();
      this->RepresentationChanged = connectActorCache->UpdateRepresentationChanges();

      // Reset geometry status and texture caches for update information from inputdatas
      connectActorCache->ResetTransferredGeometryStatus();
      connectActorCache->ResetTextureCacheStatus();

      this->Internals->SetActorProgress(rNode, 0.25);
    }
  }
  else //postpass
  {
    if (actVisible)
    {
      //
      // Scene changes for this actor (has to be performed after inputdata updates for existence of timesteps)
      //

      if(connector->GetAndResetGeomTypeChanged(actorId)) // A geom type change discards all existing clips for an actor, so make sure to purge scene to animation times as well
        connectActorCache->ResetSceneToAnimationTimes();

      bool sceneTimeChanged = connectActorCache->UpdateSceneToAnimationTime(sceneTimeStep, animTimeStep);
      if (sceneTimeChanged)
      {
        const std::vector<double>& sceneToAnimTimes = connectActorCache->GetSceneToAnimationTimes();
        size_t numSceneToAnimTimes = sceneToAnimTimes.size() / 2;
        const double* pSceneToAnim = sceneToAnimTimes.data();
        connector->SetSceneToAnimTime(actorId, sceneTimeStep, pSceneToAnim, numSceneToAnimTimes);
      }

      this->Internals->SetActorProgress(rNode, 0.35);

      //
      // Geometries that aren't part of this timestep should be fully deleted or made invisible from this timestep onward, 
      // if still existent in other timesteps.
      //

      for( int i = 0; i < connectActorCache->GetNumTransferredGeometries(); )
      {
        const vtkOmniConnectActorCache::TransferredGeomInfo& geom = connectActorCache->GetTransferredGeometry(i);

        if (geom.Status == OmniConnectStatusType::REMOVE)
        {
          this->GeometryChanged = true;

          // When no timesteps are remaining, the geometry is completely removed
          bool geomFullyDeleted = connector->DeleteGeomAtTime(actorId, animTimeStep, geom.GeomId, geom.GeomType);
          if (geomFullyDeleted)
          {
            connectActorCache->DeleteTransferredGeometry(i);
            continue;
          }
          else
          {
            connector->SetGeomVisibility(actorId, geom.GeomId, geom.GeomType, false, animTimeStep);
          }
        }
        ++i;
      }

      this->Internals->SetActorProgress(rNode, 0.5);

      //
      // Texture changes
      //

      if (this->MaterialsChanged) // If materials haven't changed, no texture removal should take place (the tagging has likely not even been performed)
      {
        for (int i = 0; i < connectActorCache->GetNumTextureCaches(); )
        {
          const vtkOmniConnectTextureCache& texCache = connectActorCache->GetTextureCache(i);

          if (texCache.Status == OmniConnectStatusType::REMOVE &&
            !connectActorCache->IsTextureReferenced(texCache.TexId))
          {
            connector->DeleteTexture(actorId, i);
            connectActorCache->DeleteTextureCache(i);

            this->TextureChanged = true;
          }
          else
            ++i;
        }
      }

      this->Internals->SetActorProgress(rNode, 0.7);

      //
      // Flush changes if necessary
      //

      // Check for all changes that invoke connector calls. (Except texture/sceneTimeChanged, which don't cause actor stage changes)
      bool actorChanged = this->VisibilityChanged || this->TransformChanged || this->GeometryChanged || this->MaterialsChanged || this->InputDataChanged;
      if (actorChanged)
      {
        connector->FlushActorUpdates(actorId);
      }
      bool sceneChanged = actorChanged || sceneTimeChanged || this->TextureChanged;
      if (sceneChanged)
      {
        rNode->FlushSceneUpdates();
      }

      this->Internals->SetActorProgress(rNode, 0.95);
    }
  }
}

//------------------------------------------------------------------------------
void vtkOmniConnectActorNodeBase::SetSubGeomProgress(double subGeomPercentage)
{
  vtkOmniConnectRendererNode* rNode = this->RendererNode;
  
  if (subGeomPercentage == 0.0)
    this->Internals->NumSubGeomsProcessed++;
  
  double percentageSubGeomsComplete = (double)this->Internals->NumSubGeomsProcessed / (double)this->Internals->NumberOfSubGeoms;
  this->Internals->SetSubGeomProgress(rNode, percentageSubGeomsComplete + subGeomPercentage);
}

//------------------------------------------------------------------------------
bool vtkOmniConnectActorNodeBase::ConnectorIsInitialized() const
{
  return this->Internals->IsInitialized();
}

//------------------------------------------------------------------------------
void vtkOmniConnectActorNodeBase::SetVtkProp3D(vtkObject* obj)
{
  if(vtkProp3D* prop3D = vtkProp3D::SafeDownCast(obj))
    VtkProp3D = prop3D;
}

void vtkOmniConnectActorNodeBase::DeleteActor()
{
  assert(ConnectorIsInitialized());
  this->RendererNode->GetOmniConnector()->DeleteActor(this->Internals->ConnectActorCache->GetId());
  this->RendererNode->FlushSceneUpdates();

  this->Internals->Reset(); // also reset the cache
}
