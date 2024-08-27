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

#ifndef OmniConnectCaches_h
#define OmniConnectCaches_h

#include <string>
#include <map>

#include "OmniConnectData.h"

struct OmniConnectActorCache;

struct OmniConnectMatCache
{
  const OmniConnectActorCache* ActorCache;

  std::string MatName;
  SdfPath SdfMatName;
  SdfPath ShadPath;

  SdfPath TexCoordReaderPath;
  SdfPath VertexColorReaderPath;
  SdfPath TextureReaderPath;

#ifdef USE_MDL_MATERIALS
  SdfPath MdlShadPath;

#if !USE_CUSTOM_MDL
#if USE_CUSTOM_POINT_SHADER
  SdfPath MdlPointShadPath;
#endif
  SdfPath TexCoordReaderPath_mdl;
  SdfPath VertexColorReaderPath_mdl;
  SdfPath VertexOpacityReaderPath_mdl;
  SdfPath VertexOpacityPowPath_mdl;
  SdfPath SamplerPath_mdl;
  SdfPath SamplerXyzPath_mdl;
  SdfPath SamplerColorPath_mdl;
  SdfPath SamplerOpacityPath_mdl;
  SdfPath OpacityMulPath_mdl;
#endif
#endif

#ifdef USE_INDEX_MATERIALS
  SdfPath IndexShadPath;
  SdfPath ColorMapShadPath;
#endif

  bool TimeVarying = false;

  void SetPathsAndNames(const OmniConnectActorCache& actorCache, size_t matId);
};

struct OmniConnectTexCache
{
  static const char* ImageExt;

  std::string AbsTexturePathBase; //abstexturepath without extension
  std::string AbsTexturePath; //abstexturepath
  std::string SceneRelTexturePathBase; //reltexpath without extension
  SdfAssetPath SceneRelTexturePath; //reltexturepath
  SdfPath TexturePrimPath;
#if !USE_CUSTOM_MDL
  SdfPath TexturePrimPath_mdl;
#endif

  std::string TimedAbsTexturePath;
  SdfAssetPath TimedSceneRelTexturePath;

  bool TimeVarying = false;
  std::vector<double> TimeSteps;

  OmniConnectSamplerData::WrapMode WrapS = OmniConnectSamplerData::WrapMode::BLACK;
  OmniConnectSamplerData::WrapMode WrapT = OmniConnectSamplerData::WrapMode::BLACK;

  void SetPathsAndNames(const OmniConnectActorCache& actorCache, size_t texId);
  void ResetTimedTexPaths(double animTimeStep);
};

struct OmniConnectGeomCache
{
  const OmniConnectActorCache* ActorCache;

  std::string GeomPath;
  SdfPath SdfGeomPath;
  std::string GeomStageBaseName;
  std::string SceneRelGeomTopologyFileBase;
  std::string SceneRelGeomTopologyFile;
  std::string GeomTopologyStagePath;

  std::string TimedSceneRelGeomClipFile;
  std::string TimedGeomClipStagePath;

  std::string TempClipUrl;

  //Persistently keeps track of this geom's clip state in actor stage
  VtArray<SdfAssetPath> ClipAssetPaths;
  VtArray<GfVec2d> ClipActives;

  //Tracks whether the AltUsdPrimType has been chosen as prim
  bool UsesAltUsdPrimType = false;
  bool HasPrivateMaterial = false;

  void SetPathsAndNamesBase(const OmniConnectActorCache& actorCache, size_t geomId, 
    const std::string& geomBaseName, const char* stagePostFix);
  void ResetTopologyFile(const OmniConnectActorCache& actorCache, const std::string& fileExtension);
  void ResetTimedGeomClipFile(double animTimeStep, const std::string& fileExtension);
  void ResetTempClipUrl(const std::string& clipAssetPath);

};

struct OmniConnectMeshCache : public OmniConnectGeomCache
{
  typedef OmniConnectMeshData GeomDataType;

  void SetPathsAndNames(const OmniConnectActorCache& actorCache, size_t meshId);
};

struct OmniConnectInstancerCache : public OmniConnectGeomCache
{
  typedef OmniConnectInstancerData GeomDataType;

  std::string ProtoPath;
  SdfPath SdfProtoPath;
  SdfPath RelSpherePath;
  SdfPath RelCylinderPath;
  SdfPath RelConePath;
  SdfPath RelCubePath;
  SdfPath RelArrowPath;
  SdfPath RelExternalSourcePath;

  void SetPathsAndNames(const OmniConnectActorCache& actorCache, size_t instancerId);
};

struct OmniConnectCurveCache : public OmniConnectGeomCache
{
  typedef OmniConnectCurveData GeomDataType;

  void SetPathsAndNames(const OmniConnectActorCache& actorCache, size_t curveId);
};

struct OmniConnectVolumeCache : public OmniConnectGeomCache
{
  typedef OmniConnectVolumeData GeomDataType;

  SdfPath OvdbDensityFieldPath;
  SdfPath OvdbDiffuseFieldPath;

  SdfPath MeshVolMatPath;
  SdfPath MeshVolShadPath;

  std::string TimedRelOvdbFile;
  std::string TimedOvdbFile;

  SdfPath TempFieldPath;
  TfToken TempFieldRelToken;
  TfToken TempFieldGridToken;

  void SetPathsAndNames(const OmniConnectActorCache& actorCache, size_t volumeId);
  void ResetTimedOvdbFile(double animTimeStep);
  void ResetTempFieldPathAndTokens(const char* name);
};

struct OmniConnectActorCache
{
  const char* FileExtension = nullptr;

  UsdStageRefPtr Stage;
  std::pair<UsdStageRefPtr, UsdStageRefPtr> LiveEditUsdStage;

  std::string UniqueName;
  std::string UniquePath;
  std::string ActorPrimPath;
  SdfPath SdfActorPrimPath;
  std::string MatBaseName;
  std::string TexBaseName;
  std::string MeshBaseName;
  std::string InstancerBaseName;
  std::string CurveBaseName;
  std::string VolumeBaseName;

  std::string OutputFile;
  std::string OutputPath; // path in which usd/mdl/texture files for this actor are output
  std::string UsdOutputFilePath; // full path name of actor usd file
  std::string UsdOutputFileUrl; // full path name of actor usd file

  std::map < size_t, OmniConnectMatCache > MatCaches;
  std::map < size_t, OmniConnectMeshCache > MeshCaches;
  std::map < size_t, OmniConnectInstancerCache > InstancerCaches;
  std::map < size_t, OmniConnectCurveCache > CurveCaches;
  std::map < size_t, OmniConnectVolumeCache > VolumeCaches;

  std::vector<std::pair<size_t, OmniConnectTexCache>> TexCaches;
  std::string SceneRelTexturePathBase; //reltexturepathbase
  std::string TexturePathBase; //abstexturepathbase

  bool GeomTypeChanged = false;

  //For scene->anim time arrays on scene prims
  VtVec2dArray TempNewClipActives;
  VtVec2dArray TempClipTimes;

  void SetPathsAndNames(size_t actorId, const char* actorName, std::string& sceneDirectory, 
    std::string& rootPrimName, std::string& actScopeName, const std::string& matScopeName, const std::string& texScopeName,
    const char* fileExt, const OmniConnectEnvironment& omniEnv);

  template<typename CacheType>
  std::map <size_t, CacheType>& GetGeomCaches() {}

  using MatCachePair = std::pair<bool, OmniConnectMatCache&>;
  MatCachePair GetMatCache(size_t matId)
  {
    auto itSuccessPair = MatCaches.emplace(matId, OmniConnectMatCache());
    bool isNew = itSuccessPair.second;
    OmniConnectMatCache& matCache = (*itSuccessPair.first).second;
    if (isNew)
    { // insertion succeeded, setup some default paths
      matCache.SetPathsAndNames(*this, matId);
    }
    return MatCachePair(isNew, matCache);
  }

  using TexCachePair = std::pair<bool, OmniConnectTexCache&>;
  TexCachePair GetTexCache(size_t texId)
  {
    for (auto& x : TexCaches)
    {
      if (x.first == texId)
        return TexCachePair(false, x.second);
    }
    TexCaches.emplace_back(std::make_pair(texId,OmniConnectTexCache()));
    TexCaches.back().second.SetPathsAndNames(*this, texId);
    return TexCachePair(true, TexCaches.back().second);
  }

  template<typename CacheType>
  std::pair<bool, CacheType&> GetGeomCache(size_t geomId)
  {
    auto itSuccessPair = GetGeomCaches<CacheType>().emplace(geomId, CacheType());
    bool isNew = itSuccessPair.second;
    CacheType& geomCache = (*itSuccessPair.first).second;
    if (isNew)
    { // insertion succeeded, setup some default paths
      geomCache.SetPathsAndNames(*this, geomId);
    }
    return std::pair<bool, CacheType&>(isNew, geomCache);
  }
};

using ActorCacheMapType = std::map<size_t, OmniConnectActorCache>;

template<>
std::map <size_t, OmniConnectMeshCache>& OmniConnectActorCache::GetGeomCaches<OmniConnectMeshCache>();

template<>
std::map <size_t, OmniConnectInstancerCache>& OmniConnectActorCache::GetGeomCaches<OmniConnectInstancerCache>();

template<>
std::map <size_t, OmniConnectCurveCache>& OmniConnectActorCache::GetGeomCaches<OmniConnectCurveCache>();

template<>
std::map <size_t, OmniConnectVolumeCache>& OmniConnectActorCache::GetGeomCaches<OmniConnectVolumeCache>();

#endif