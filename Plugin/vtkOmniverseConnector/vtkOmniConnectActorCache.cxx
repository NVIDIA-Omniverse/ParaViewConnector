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

#include "vtkOmniConnectActorCache.h"
#include "vtkOmniConnectRendererNode.h"
#include "vtkActor.h"
#include "vtkVolume.h"
#include "vtkProperty.h"
#include "vtkVolumeProperty.h"
#include "vtkInformation.h"
#include "vtkMatrix4x4.h"
#include "vtkImageData.h"
#include "vtkTexture.h"
#include "vtkPolyDataMapper.h"
#include "vtkVolumeMapper.h"
#include "OmniConnectUtilsExternal.h"

typedef vtkOmniConnectRendererNode OmniInformationBase;

namespace
{
  void CleanupActorName(std::string& name)
  {
    if (name.length() > 0)
      ocutils::FormatUsdName(const_cast<char*>(name.data())); // Could pass library boundaries, so char*
  }

  vtkAbstractMapper3D* GetVtkMapper(vtkProp3D* prop3D)
  {
    if (vtkActor* actor = vtkActor::SafeDownCast(prop3D))
    {
      return actor->GetMapper();
    }
    else if (vtkVolume* volume = vtkVolume::SafeDownCast(prop3D))
    {
      return volume->GetMapper();
    }
    return nullptr;
  }

  vtkObject* GetVtkProperty(vtkProp3D* prop3D)
  {
    if (vtkActor* actor = vtkActor::SafeDownCast(prop3D))
    {
      return actor->GetProperty();
    }
    else if (vtkVolume* volume = vtkVolume::SafeDownCast(prop3D))
    {
      return volume->GetProperty();
    }
    return nullptr;
  }

  vtkTexture* GetActorTexture(vtkProp3D* prop3D)
  {
    if (vtkActor* actor = vtkActor::SafeDownCast(prop3D))
    {
      return actor->GetTexture();
    }
    return nullptr;
  }
}

vtkOmniConnectActorCache::vtkOmniConnectActorCache()
{}

vtkOmniConnectActorCache::vtkOmniConnectActorCache(vtkProp3D* prop3D, size_t actorId)
  : VtkProp3D(prop3D), ActorId(actorId)
{}

void vtkOmniConnectActorCache::Initialize()
{
  std::string newName = GenerateName();
  if(ActorName != newName)
  {
    vtkInformation* info = VtkProp3D->GetPropertyKeys();
    ActorNameSource = info->Get(OmniInformationBase::ACTORNAME()); // Store the pointer as an ID
    ActorName = newName;
  }
}

std::string vtkOmniConnectActorCache::GenerateName() const
{
  std::string resultName;
  // Decide on the actor name
  vtkInformation* info = VtkProp3D->GetPropertyKeys();
  if (info && info->Has(OmniInformationBase::ACTORNAME()))
  {
    resultName = info->Get(OmniInformationBase::ACTORNAME());
    CleanupActorName(resultName);
  }
  else
  {
    resultName = std::string("someActorName_") + std::to_string(ActorId);
  }
  return resultName;
}

bool vtkOmniConnectActorCache::NameHasChanged() const
{
  vtkInformation* info = VtkProp3D->GetPropertyKeys();
  const char* infoName = info->Get(OmniInformationBase::ACTORNAME());
  if(ActorNameSource.compare(infoName))
  {
    return true;
  }
  return false;
}

bool vtkOmniConnectActorCache::UpdateVisibilityChanges()
{
  bool visible = VtkProp3D->GetVisibility();
  bool change = visible != this->Visibility;
  this->Visibility = visible;
  return change;
}

bool vtkOmniConnectActorCache::UpdateActorChanges()
{
  if (this->VtkProp3D->GetMTime() > this->VtkProp3DMTime)
  {
    this->VtkProp3DMTime = this->VtkProp3D->GetMTime();
    return true;
  }
  return false;
}

bool vtkOmniConnectActorCache::UpdateTransformChanges(vtkOmniConnectTimeStep* timeStep)
{
  return timeStep->UpdateTransform(this->VtkProp3D->GetMatrix());
}

bool vtkOmniConnectActorCache::UpdateMaterialChanges()
{
  bool changed = false;

  vtkAbstractMapper3D* mapper = GetVtkMapper(this->VtkProp3D);
  assert(mapper);
  {
    vtkMTimeType mtime = this->VtkMapperMTime;
    if (mapper->GetMTime() > mtime)
    {
      mtime = mapper->GetMTime();
    }
    if (mapper->GetInformation()->GetMTime() > mtime)
    {
      mtime = mapper->GetInformation()->GetMTime();
    }
    if (mapper != this->VtkMapper || mtime > this->VtkMapperMTime)
    {
      changed = true;
      this->VtkMapper = mapper;
      this->VtkMapperMTime = mtime;
    }
  }

  vtkObject* actProperty = GetVtkProperty(this->VtkProp3D);
  assert(actProperty);
  {
    vtkMTimeType mtime = this->VtkPropertyMTime;
    if (actProperty->GetMTime() > mtime)
    {
      mtime = actProperty->GetMTime();
    }
    if (vtkProperty* meshProp = vtkProperty::SafeDownCast(actProperty))
    {// Volumes and meshes do not share a property base class
      if (meshProp->GetInformation()->GetMTime() > mtime)
      {
        mtime = meshProp->GetInformation()->GetMTime();
      }
    }
    if (actProperty != this->VtkProperty || mtime > this->VtkPropertyMTime)
    {
      changed = true;
      this->VtkProperty = actProperty;
      this->VtkPropertyMTime = mtime;
    }
  }

  // Actor texture is not necessarily used, but at least it triggers materials to be processed to further figure out changes.
  vtkTexture* texture = GetActorTexture(this->VtkProp3D);
  if (texture != this->VtkActorTexture)
  {
    this->VtkActorTexture = texture;
    changed = true;
  }
  if (texture)
  {
    vtkMTimeType mtime = this->VtkActorTextureMTime;
    if (texture->GetMTime() > mtime)
    {
      mtime = texture->GetMTime();
    }
    if (texture->GetInput() && texture->GetInput()->GetMTime() > mtime)
    {
      mtime = texture->GetInput()->GetMTime();
    }
    if (mtime > this->VtkActorTextureMTime)
    {
      this->VtkActorTextureMTime = mtime;
      changed = true;
    }
  }

  return changed;
}

bool vtkOmniConnectActorCache::UpdateRepresentationChanges()
{
  vtkProperty* meshProp = vtkProperty::SafeDownCast(GetVtkProperty(this->VtkProp3D));
  if (meshProp)
  {
    bool change = this->VtkActorRepresentation > -1 && this->VtkActorRepresentation != meshProp->GetRepresentation();
    this->VtkActorRepresentation = meshProp->GetRepresentation();
    return change;
  }
  return false;
}

void vtkOmniConnectActorCache::ResetTransferredGeometryStatus()
{
  for (auto& entry : OmniGeometriesTransferred)
  {
    entry.Status = OmniConnectStatusType::REMOVE;
  }
}

void vtkOmniConnectActorCache::KeepTransferredGeometries()
{
  for (auto& entry : OmniGeometriesTransferred)
  {
    entry.Status = OmniConnectStatusType::KEEP;
  }
}

bool vtkOmniConnectActorCache::AddTransferredGeometry(size_t geomId, OmniConnectGeomType geomType)
{
  for (auto& entry : OmniGeometriesTransferred)
  {
    if (entry.GeomId == geomId && entry.GeomType == geomType)
    {
      //If found, make sure to keep
      entry.Status = OmniConnectStatusType::KEEP;
      return false;
    }
  }
  //If not found, create a new entry
  OmniGeometriesTransferred.emplace_back(geomId, geomType, OmniConnectStatusType::KEEP);
  return true;
}

void vtkOmniConnectActorCache::DeleteTransferredGeometry(int i)
{
  OmniGeometriesTransferred[i] = OmniGeometriesTransferred.back();
  OmniGeometriesTransferred.pop_back();
}

void vtkOmniConnectActorCache::ResetTextureCacheStatus()
{
  for (auto& cacheEntry : OmniTextureCaches)
  {
    cacheEntry.Status = OmniConnectStatusType::REMOVE; 
  }
}

vtkOmniConnectTextureCache* vtkOmniConnectActorCache::UpdateTextureCache(vtkImageData* texData, size_t materialId, bool isColorTextureMap)
{
  // First try to update an existing cache entry without changing the material reference
  int64_t refIdx = -1;
  int64_t cacheIdx = -1;
  for (size_t i = 0; i < this->MatToTextureRefs.size(); ++i)
  {
    if (MatToTextureRefs[i].first == materialId)
    {
      size_t currentTexRefId = MatToTextureRefs[i].second;
      refIdx = i;

      //Find the texture cache entry belonging to the referenced texID (must be there, reference is always valid)
      bool cacheEntryFound = false;
      for (size_t j = 0; j < this->OmniTextureCaches.size(); ++j)
      {
        if (OmniTextureCaches[j].TexId == currentTexRefId)
        {
          cacheEntryFound = true;

          // if entry has "removed" status
          if (OmniTextureCaches[j].Status == OmniConnectStatusType::REMOVE)
          {
            //If texdata is the same, determine update status
            if (OmniTextureCaches[j].TexData == texData)
            {
              OmniTextureCaches[j].DetermineUpdateRequired();
            }
            //Otherwise, change texData and set to "updated" (take over the tex id for texData)
            else
            {
              OmniTextureCaches[j].TexData = texData;
              OmniTextureCaches[j].TexMTime = texData->GetMTime();
              OmniTextureCaches[j].IsColorTextureMap = isColorTextureMap;
              OmniTextureCaches[j].Status = OmniConnectStatusType::UPDATE;
            }
            cacheIdx = j;
          }
          //if entry has already been kept/updated by another material, 
          else
          {
            //if texData is the same, texcache status is correct and the reference too, so record the current cache index.
            //if texData is not the same, we will have to update the material reference to another cache entry (either existing or new).
            if (OmniTextureCaches[j].TexData == texData)
              cacheIdx = j;
          }
        }
      }
      assert(cacheEntryFound);
    }
  }
  // If no suitable cache entry has been found, go directly through the texcache based on texData, possible creating a new entry
  if(cacheIdx == -1)
  {
    int64_t resultTexId = -1;
    for (size_t j = 0; j < this->OmniTextureCaches.size(); ++j)
    {
      // If found, determine update status of the cache entry
      if (this->OmniTextureCaches[j].TexData == texData)
      {
        resultTexId = this->OmniTextureCaches[j].TexId;
        this->OmniTextureCaches[j].DetermineUpdateRequired();
        cacheIdx = j;
      }
    }
    if (cacheIdx == -1)
    {
      //create a new cache entry with this texdata and new Id (split texdata into new cache entry)
      resultTexId = this->NewTextureId();
      cacheIdx = this->OmniTextureCaches.size();
      this->OmniTextureCaches.emplace_back(texData, isColorTextureMap, resultTexId, texData->GetMTime(), OmniConnectStatusType::UPDATE);
    }
    // After any of the above options, set or create the reference to the new cache entry.
    if (refIdx != -1)
      MatToTextureRefs[refIdx].second = resultTexId;
    else
      MatToTextureRefs.emplace_back(materialId, resultTexId);
  }
  //Texture cache entries that are not referenced anymore should be deleted after all calls to UpdateTextureCache.

  return &this->OmniTextureCaches[cacheIdx];
}

void vtkOmniConnectActorCache::DeleteTextureCache(int i)
{
  OmniTextureCaches[i] = OmniTextureCaches.back();
  OmniTextureCaches.pop_back();
}

bool vtkOmniConnectActorCache::IsTextureReferenced(int texId)
{
  for (auto matToTexId : this->MatToTextureRefs)
  {
    if (matToTexId.second == texId)
      return true;
  }
  return false;
}

int vtkOmniConnectActorCache::NewTextureId()
{
  return ++this->RunningTextureId;
}

vtkOmniConnectTimeStep* vtkOmniConnectActorCache::GetTimeStep(double timeStep)
{
  return &OmniTimeSteps[timeStep];
}

void vtkOmniConnectActorCache::ResetSceneToAnimationTimes()
{
  SceneToAnimationTimes.clear();
}

bool vtkOmniConnectActorCache::UpdateSceneToAnimationTime(double sceneTime, double animTime)
{
  int keyIdx = 0;
  //Find entry with same scenetime
  while (keyIdx < SceneToAnimationTimes.size())
  {
    if (SceneToAnimationTimes[keyIdx] == sceneTime)
      break;
    keyIdx+=2;
  }
  //if there is no such entry, insert scene->anim record and return true
  if (keyIdx == SceneToAnimationTimes.size())
  {
    SceneToAnimationTimes.push_back(sceneTime);
    SceneToAnimationTimes.push_back(animTime);
    return true;
  }
  //if the past animtime doesn't belong to the scenetime, change existing record and return true
  else if (SceneToAnimationTimes[keyIdx+1] != animTime)
  {
    SceneToAnimationTimes[keyIdx + 1] = animTime;
    return true;
  }
  //scene->anim times already up to date, return false
  return false;
}