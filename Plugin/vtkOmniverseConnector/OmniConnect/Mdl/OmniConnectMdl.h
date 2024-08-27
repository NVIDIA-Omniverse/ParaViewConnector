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

#ifndef OmniConnectMdl_h
#define OmniConnectMdl_h

#include "OmniConnectData.h"

#include "usd.h"
PXR_NAMESPACE_USING_DIRECTIVE

#ifdef USE_MDL_MATERIALS

class OmniConnectConnection;
struct OmniConnectActorCache;
struct OmniConnectMatCache;
struct OmniConnectTexCache;

struct OmniConnectMdlNames
{
#if USE_CUSTOM_MDL || USE_CUSTOM_POINT_SHADER
  SdfAssetPath OpaqueMdl;
  SdfAssetPath TranslucentMdl;
#endif
#if !USE_CUSTOM_MDL
  SdfAssetPath PbrMdl;
#endif
  SdfAssetPath MeshVolumeMdl;
};

void CreateMdlTemplates(OmniConnectConnection* OmniConnect, OmniConnectMdlNames& mdlNames, const std::string& sceneDirectory, const std::string& materialPath);

UsdShadeOutput CreateUsdMdlSurfaceShader(const OmniConnectActorCache& actorCache, OmniConnectMatCache& matCache, UsdShadeShader& usdShader, OmniConnectMdlNames& mdlNames, bool newMat
#if USE_CUSTOM_POINT_SHADER
  , bool createCustomShader = false
#endif
);
UsdShadeOutput CreateUsdMdlVolumeShader(UsdShadeShader& usdShader);
UsdShadeOutput CreateUsdMdlMeshVolumeShader(UsdShadeShader& usdShader, OmniConnectMdlNames& mdlNames);

void ResetUsdMdlShader(UsdShadeShader& usdShader, OmniConnectActorCache& actorCache, OmniConnectMatCache& matCache);

void UpdateUsdMdlShader(OmniConnectActorCache& actorCache, OmniConnectMatCache& matCache, const OmniConnectTexCache* texCache, 
  const OmniConnectMaterialData& omniMatData, UsdShadeShader& usdShader, const OmniConnectMdlNames& mdlNames, double animTimeStep
#if USE_CUSTOM_POINT_SHADER
  , bool updateCustomShader = false
#endif
  );

#if !USE_CUSTOM_MDL
UsdShadeShader InitializeMdlTexture(const OmniConnectActorCache& actorCache, const OmniConnectSamplerData& samplerData, const OmniConnectTexCache& texCache);
void UpdateMdlTexture(const OmniConnectTexCache& texCache, const UsdShadeShader& sampler, double animTimeStep);
#endif

#endif

#ifdef USE_INDEX_MATERIALS
UsdShadeOutput CreateUsdIndexVolumeShader(OmniConnectActorCache& actorCache, OmniConnectMatCache& matCache, UsdShadeShader& indexShader, bool newMat);
void ResetUsdIndexVolumeShader(OmniConnectActorCache& actorCache, OmniConnectMatCache& matCache);
void UpdateUsdIndexVolumeShader(OmniConnectActorCache& actorCache, OmniConnectMatCache& matCache, const OmniConnectTexCache* texCache, 
  const OmniConnectMaterialData& omniMatData, UsdShadeShader& indexShader, double animTimeStep);
#endif

#endif
