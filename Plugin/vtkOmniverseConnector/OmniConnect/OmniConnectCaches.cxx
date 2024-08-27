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

#include "usd.h"
PXR_NAMESPACE_USING_DIRECTIVE

#include "OmniConnectCaches.h"
#include "OmniConnectUtilsExternal.h"

extern const char* OmniTextureRelativePath;
extern const char* OmniMaterialRelativePath;
extern const char* OmniGeomRelativePath;
extern const char* OmniTopologyRelativePath;
extern const char* OmniVolumeRelativePath;

const char* OmniConnectTexCache::ImageExt = ".png";

namespace constring
{
  const char* usdShaderName = "/UsdShader";
#ifdef USE_MDL_MATERIALS
  const char* mdlShaderName = "/MdlShader";
#endif
#ifdef USE_INDEX_MATERIALS
  const char* indexShaderName = "/IndexShader";
#endif
}

template<>
std::map <size_t, OmniConnectMeshCache>& OmniConnectActorCache::GetGeomCaches<OmniConnectMeshCache>() { return MeshCaches; }

template<>
std::map <size_t, OmniConnectInstancerCache>& OmniConnectActorCache::GetGeomCaches<OmniConnectInstancerCache>() { return InstancerCaches; }

template<>
std::map <size_t, OmniConnectCurveCache>& OmniConnectActorCache::GetGeomCaches<OmniConnectCurveCache>() { return CurveCaches; }

template<>
std::map <size_t, OmniConnectVolumeCache>& OmniConnectActorCache::GetGeomCaches<OmniConnectVolumeCache>() { return VolumeCaches; }

void OmniConnectActorCache::SetPathsAndNames(size_t, const char* actorName, std::string& sceneDirectory, 
  std::string& rootPrimName, std::string& actScopeName, const std::string& matScopeName, const std::string& texScopeName,
  const char* fileExt, const OmniConnectEnvironment& omniEnv)
{
  this->FileExtension = fileExt;

  this->UniqueName = actorName;// +std::to_string(actorId);
  if (omniEnv.NumProcs > 1)
    this->UniqueName += "_Proc_" + std::to_string(omniEnv.ProcId);

  this->UniquePath = "/" + this->UniqueName;
  this->ActorPrimPath = actScopeName + this->UniquePath;
  this->SdfActorPrimPath = SdfPath(this->ActorPrimPath);
  this->MatBaseName = matScopeName + this->UniquePath + "_Mat_";
  this->TexBaseName = texScopeName + this->UniquePath + "_Tex_";
  this->MeshBaseName = this->ActorPrimPath + "/MeshGeom_";
  this->InstancerBaseName = this->ActorPrimPath + "/InstancerGeom_";
  this->CurveBaseName = this->ActorPrimPath + "/CurveGeom_";
  this->VolumeBaseName = this->ActorPrimPath + "/VolumeGeom_";

  this->OutputPath = sceneDirectory;

  this->OutputFile = this->UniqueName + this->FileExtension;
  this->UsdOutputFilePath = this->OutputPath + this->OutputFile;

  this->SceneRelTexturePathBase = "./" + (OmniTextureRelativePath + this->UniqueName + "_");
  this->TexturePathBase = this->OutputPath + this->SceneRelTexturePathBase.substr(2);
}

void OmniConnectMatCache::SetPathsAndNames(const OmniConnectActorCache& actorCache, size_t matId)
{
  this->ActorCache = &actorCache;

  this->MatName = actorCache.MatBaseName + std::to_string(matId);
  this->SdfMatName = SdfPath(this->MatName);

  this->ShadPath = SdfPath(this->MatName + constring::usdShaderName);

  const char* vcReaderPath = "/vertexcolorreader";
  const char* tcReaderPath = "/texcoordreader";
  const char* texReaderPath = "/texturereader";
  this->VertexColorReaderPath = SdfPath(this->MatName + vcReaderPath);
  this->TexCoordReaderPath = SdfPath(this->MatName + tcReaderPath);
  this->TextureReaderPath = SdfPath(this->MatName + texReaderPath);

#ifdef USE_MDL_MATERIALS
  this->MdlShadPath = SdfPath(this->MatName + constring::mdlShaderName);
#if !USE_CUSTOM_MDL
#if USE_CUSTOM_POINT_SHADER
  this->MdlPointShadPath = SdfPath(this->MatName + constring::mdlShaderName + "_points");
#endif
  const char* mdlPostfix = "_mdl";
  this->VertexColorReaderPath_mdl = SdfPath(this->MatName + "/vertexcolorreader" + mdlPostfix);
  this->VertexOpacityReaderPath_mdl = SdfPath(this->MatName + "/vertexopacityreader" + mdlPostfix);
  this->VertexOpacityPowPath_mdl = SdfPath(this->MatName + "/vertexopacitypow" + mdlPostfix);
  this->TexCoordReaderPath_mdl = SdfPath(this->MatName + "/texcoordreader" + mdlPostfix);
  this->SamplerPath_mdl = SdfPath(this->MatName + "/texturereader" + mdlPostfix);
  this->SamplerXyzPath_mdl = SdfPath(this->MatName + "/texture_xyz" + mdlPostfix);
  this->SamplerColorPath_mdl = SdfPath(this->MatName + "/texture_color" + mdlPostfix);
  this->SamplerOpacityPath_mdl = SdfPath(this->MatName + "/texture_opacity" + mdlPostfix);
  this->OpacityMulPath_mdl = SdfPath(this->MatName + "/opacity_mul" + mdlPostfix);
#endif
#endif

#ifdef USE_INDEX_MATERIALS
  this->IndexShadPath = SdfPath(this->MatName + constring::indexShaderName);
  this->ColorMapShadPath = SdfPath(this->MatName + constring::indexShaderName + "_colormap");
#endif
}

void OmniConnectTexCache::SetPathsAndNames(const OmniConnectActorCache& actorCache, size_t texId)
{
  std::string idStr = std::to_string(texId);
  this->AbsTexturePathBase = actorCache.TexturePathBase + idStr;
  this->AbsTexturePath = this->AbsTexturePathBase + ImageExt;

  this->SceneRelTexturePathBase = actorCache.SceneRelTexturePathBase + idStr;
  this->SceneRelTexturePath = SdfAssetPath(this->SceneRelTexturePathBase + ImageExt);
  std::string texPrimPath = actorCache.TexBaseName + idStr;
  this->TexturePrimPath = SdfPath(texPrimPath);
#if !USE_CUSTOM_MDL
  this->TexturePrimPath_mdl = SdfPath(texPrimPath + "_mdl");
#endif
}

void OmniConnectTexCache::ResetTimedTexPaths(double animTimeStep)
{
  std::string timedExt = "." + std::to_string(animTimeStep) + OmniConnectTexCache::ImageExt;
  TimedAbsTexturePath = this->AbsTexturePathBase + timedExt;
  TimedSceneRelTexturePath = SdfAssetPath(this->SceneRelTexturePathBase + timedExt);
}

void OmniConnectGeomCache::ResetTopologyFile(const OmniConnectActorCache& actorCache, const std::string& fileExtension)
{
  this->SceneRelGeomTopologyFile = this->SceneRelGeomTopologyFileBase + fileExtension;
  this->GeomTopologyStagePath = actorCache.OutputPath + this->SceneRelGeomTopologyFile;
}

void OmniConnectGeomCache::ResetTimedGeomClipFile(double animTimeStep, const std::string& fileExtension)
{
  this->TimedSceneRelGeomClipFile = OmniGeomRelativePath + this->GeomStageBaseName + "." + std::to_string(animTimeStep) + fileExtension;
  this->TimedGeomClipStagePath = this->ActorCache->OutputPath + this->TimedSceneRelGeomClipFile;
}

void OmniConnectGeomCache::ResetTempClipUrl(const std::string& clipAssetPath)
{
  this->TempClipUrl = this->ActorCache->OutputPath + clipAssetPath;
}

void OmniConnectGeomCache::SetPathsAndNamesBase(const OmniConnectActorCache& actorCache, size_t geomId, const std::string& geomBaseName, const char* stagePostFix)
{
  this->ActorCache = &actorCache;
  size_t postfixLength = STRNLEN_PORTABLE(this->ActorCache->FileExtension, MAX_USER_STRLEN);

  this->GeomPath = geomBaseName + std::to_string(geomId);
  this->SdfGeomPath = SdfPath(this->GeomPath);
  this->GeomStageBaseName = actorCache.OutputFile.substr(0, actorCache.OutputFile.length() - postfixLength) + "_" + std::to_string(geomId) + stagePostFix;
  this->SceneRelGeomTopologyFileBase = OmniTopologyRelativePath + this->GeomStageBaseName + "Topology";
}

void OmniConnectMeshCache::SetPathsAndNames(const OmniConnectActorCache& actorCache, size_t meshId)
{
  SetPathsAndNamesBase(actorCache, meshId, actorCache.MeshBaseName, "_MeshGeom_");

  /*
  this->MeshName = actorCache.MeshBaseName + std::to_string(meshId);
  this->MeshPath = SdfPath(actorCache.MeshBaseName + std::to_string(meshId));
  this->MeshGeomBaseName = actorCache.OutputFile.substr(0, actorCache.OutputFile.length() - postfixLength) + "_" + std::to_string(meshId) + "_MeshGeom_";
  this->SceneRelMeshGeomTopologyFile = this->MeshGeomBaseName + "Topology" + this->ActorCache->FileExtension;
  this->MeshGeomTopologyFilePath = actorCache.OutputPath + this->SceneRelMeshGeomTopologyFile;
  */
}

void OmniConnectInstancerCache::SetPathsAndNames(const OmniConnectActorCache& actorCache, size_t instancerId)
{
  SetPathsAndNamesBase(actorCache, instancerId, actorCache.InstancerBaseName, "_InstancerGeom_");

  this->ProtoPath = this->GeomPath + "/Prototypes";
  this->SdfProtoPath = SdfPath(this->ProtoPath);
  this->RelSpherePath = SdfPath("Sphere");
  this->RelCylinderPath = SdfPath("Cylinder");
  this->RelConePath = SdfPath("Cone");
  this->RelCubePath = SdfPath("Cube");
  this->RelArrowPath = SdfPath("Arrow");
  this->RelExternalSourcePath = SdfPath("ExternalSource");
}

void OmniConnectCurveCache::SetPathsAndNames(const OmniConnectActorCache & actorCache, size_t curveId)
{
  SetPathsAndNamesBase(actorCache, curveId, actorCache.CurveBaseName, "_CurveGeom_");
}

void OmniConnectVolumeCache::SetPathsAndNames(const OmniConnectActorCache& actorCache, size_t volumeId)
{
  SetPathsAndNamesBase(actorCache, volumeId, actorCache.VolumeBaseName, "_VolumeGeom_");

  this->OvdbDensityFieldPath = this->SdfGeomPath.AppendPath(SdfPath("ovdbdensityfield"));
  this->OvdbDiffuseFieldPath = this->SdfGeomPath.AppendPath(SdfPath("ovdbdiffusefield"));

  this->MeshVolMatPath = this->SdfGeomPath.AppendPath(SdfPath("volmat"));
  this->MeshVolShadPath = this->MeshVolMatPath.AppendPath(SdfPath("volshad"));
}

void OmniConnectVolumeCache::ResetTimedOvdbFile(double animTimeStep)
{
  this->TimedRelOvdbFile = OmniVolumeRelativePath + this->GeomStageBaseName + "." + std::to_string(animTimeStep) + ".vdb";
  this->TimedOvdbFile = this->ActorCache->OutputPath + TimedRelOvdbFile;
}

void OmniConnectVolumeCache::ResetTempFieldPathAndTokens(const char* name)
{
  this->TempFieldGridToken = TfToken(name);
  std::string formattedName(name);
  ocutils::FormatUsdName(const_cast<char*>(formattedName.data()));
  this->TempFieldRelToken = TfToken(formattedName);

  this->TempFieldPath = this->SdfGeomPath.AppendPath(SdfPath(formattedName));
}