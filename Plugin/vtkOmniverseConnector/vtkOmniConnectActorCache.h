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

#ifndef vtkOmniConnectActorCache_h
#define vtkOmniConnectActorCache_h

#include "vtkOmniConnectTimeStep.h"
#include "vtkOmniConnectTextureCache.h"

class vtkProp3D;
class vtkProperty;
class vtkTexture;
class vtkAbstractMapper3D;

struct vtkOmniConnectActorCache
{
public:
  struct TransferredGeomInfo
  {
    TransferredGeomInfo(size_t geomId, OmniConnectGeomType geomType, OmniConnectStatusType status)
      : GeomId(geomId), GeomType(geomType), Status(status) {}

    size_t GeomId = 0;
    OmniConnectGeomType GeomType = OmniConnectGeomType::MESH;
    OmniConnectStatusType Status = OmniConnectStatusType::REMOVE;
  };

  vtkOmniConnectActorCache(); 
  vtkOmniConnectActorCache(vtkProp3D* prop, size_t actorId);
 
  vtkOmniConnectActorCache(vtkOmniConnectActorCache&& other) = default;
  ~vtkOmniConnectActorCache() = default;
  vtkOmniConnectActorCache& operator=(vtkOmniConnectActorCache&& rhs) = default;

  void Initialize(); // If already initialized, returns whether name is still up-to-date

  std::string GenerateName() const;
  bool NameHasChanged() const;

  //vtkActor* Actor() const { return VtkActor; }
  size_t GetId() const { return ActorId; }

  bool UpdateVisibilityChanges();
  bool UpdateActorChanges();
  bool UpdateTransformChanges(vtkOmniConnectTimeStep* timeStep);
  bool UpdateMaterialChanges();
  bool UpdateRepresentationChanges();

  void ResetTransferredGeometryStatus(); // resets both instancers and meshes
  void KeepTransferredGeometries();
  bool AddTransferredGeometry(size_t geomId, OmniConnectGeomType geomType);
  size_t GetNumTransferredGeometries() const { return OmniGeometriesTransferred.size(); }
  const TransferredGeomInfo& GetTransferredGeometry(int i) const { return OmniGeometriesTransferred[i]; }
  void DeleteTransferredGeometry(int i);

  void ResetTextureCacheStatus();
  vtkOmniConnectTextureCache* UpdateTextureCache(vtkImageData* texData, size_t materialId, bool isColorTextureMap);
  size_t GetNumTextureCaches() const { return OmniTextureCaches.size(); }
  const vtkOmniConnectTextureCache& GetTextureCache(int i) const { return OmniTextureCaches[i]; }
  void DeleteTextureCache(int i);
  bool IsTextureReferenced(int texId);
  int NewTextureId();

  vtkOmniConnectTimeStep* GetTimeStep(double timeStep);
  const std::string& GetActorName() const { return ActorName; }

  void ResetSceneToAnimationTimes();
  bool UpdateSceneToAnimationTime(double sceneTime, double animTime);
  const std::vector<double>& GetSceneToAnimationTimes() const { return SceneToAnimationTimes; }
  std::vector<double>& GetSceneToAnimationTimes() { return SceneToAnimationTimes; }

protected: 
  size_t ActorId = 0;
  std::string ActorName;
  std::string ActorNameSource;
  
  bool Visibility = true;

  vtkProp3D* VtkProp3D = nullptr; // vtkActor/vtkVolume
  vtkMTimeType VtkProp3DMTime = 0;

  vtkAbstractMapper3D* VtkMapper = nullptr;
  vtkMTimeType VtkMapperMTime = 0;
  vtkObject* VtkProperty = nullptr; // vtkVolume does not subclass vtkProperty
  vtkMTimeType VtkPropertyMTime = 0;
  
  // Only applicable to vtkActor
  vtkTexture* VtkActorTexture = nullptr; 
  vtkMTimeType VtkActorTextureMTime = 0;
  int VtkActorRepresentation = -1;

  // Caches
  std::vector<vtkOmniConnectTextureCache> OmniTextureCaches;
  std::vector<std::pair<size_t, int>> MatToTextureRefs;
  std::vector<TransferredGeomInfo> OmniGeometriesTransferred; //All mesh/instancer ids currently added to OmniConnect, over all timesteps
  std::map<double, vtkOmniConnectTimeStep> OmniTimeSteps; // Per-timestep cache
  std::vector<double> SceneToAnimationTimes; // Mapping of scene times to this actor's animation times

private:
  int RunningTextureId = 0;
};

typedef std::unique_ptr<vtkOmniConnectActorCache> OmniConnectActorPtr;

#endif
