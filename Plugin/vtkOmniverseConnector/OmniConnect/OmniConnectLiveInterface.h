/*###############################################################################
#
# SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: MIT
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

#ifndef OmniConnectLiveInterface_h
#define OmniConnectLiveInterface_h

#include "usd.h"

PXR_NAMESPACE_USING_DIRECTIVE

#include "OmniConnectCaches.h"
#include "OmniConnectConnection.h"
#include "OmniConnectLiveSession.h"

#include <cstdint>
#include <map>

class OmniConnectLiveInterface
{
  public:
    void Initialize(OmniConnectConnection* connection, OmniConnectLogCallback logCallback, std::string* usdExtension, std::string* rootPrimName);
    bool IsInitialized() const { return Connection != nullptr; }

    // Enable/disable live behavior
    void SetLiveWorkflow(bool enable, UsdStageRefPtr& multiSceneStage, UsdStageRefPtr& sceneStage,
      std::string& multiSceneStageUrl, std::string& sceneStageUrl,
      std::string& multiSceneFileName, std::string& sceneFileName,
      ActorCacheMapType* actorCaches);
    bool GetLiveWorkflowEnabled() const { return LiveWorkflowEnabled; }

    void ResetTopologyFile(const OmniConnectActorCache& actorCache, OmniConnectGeomCache& geomCache);
    void ResetTimedGeomClipFile(const OmniConnectActorCache& actorCache, OmniConnectGeomCache& geomCache,
      double animTimeStep, bool createRevision = true);
    void InitializeLiveLayer(UsdStageRefPtr& liveStage) const;
    // Work around stale Kit geometry by publishing a new, completed live clip.
    bool PublishLiveClip(OmniConnectActorCache& actorCache,
      OmniConnectGeomCache& geomCache, double animTimeStep);
    SdfAssetPath GetAssetPathForAuthoring(const UsdStageRefPtr& durableStage,
      const SdfAssetPath& durableAssetPath) const;
    void FlushStage(UsdStageRefPtr& stage);
    void FlushActorStage(OmniConnectActorCache& actorCache);
    size_t UpdateClipAssetPath(VtArray<SdfAssetPath>& assetPaths,
      const std::string& assetPath, bool& changed) const;
    bool ModifyClipActives(VtVec2dArray& clipActives, double animTimeStep,
      double assetPathIdx, bool& changed) const;
    
    const std::string& GetLiveExtension() const { return LiveWorkflowEnabled ? LiveExtension : *UsdExtension; }
    const OmniConnectLiveSessionState& GetLiveSession() const { return LiveSession; }

    static OmniConnectLogCallback LogCallback;

  protected:
    bool StartLiveSession(const std::string& baseLayerUrl);
    void StopLiveSession();
    UsdStageRefPtr CreateSessionRootLayer();
    UsdStageRefPtr CreateLiveEditLayer(UsdStageRefPtr& stage, const std::string& durableLayerUrl,
      const std::string& layerNamePrefix, OmniConnectLiveLayerRole role);
    bool AttachLiveEditLayer(UsdStageRefPtr& stage, const UsdStageRefPtr& liveEditStage);
    bool AddSubLayerToParent(UsdStageRefPtr parentStage, UsdStageRefPtr childStage);
    bool ClearLiveSubLayers(const UsdStageRefPtr& stage);
    std::string MakeLiveLayerName(const std::string& prefix, const std::string& stableIdentity) const;

    void DetachLiveEditLayer(UsdStageRefPtr& stage, UsdStageRefPtr& liveEditStage);
    bool MergeLiveEditLayer(UsdStageRefPtr& stage, UsdStageRefPtr& liveEditStage);
    void CopyLayerToLive(UsdStageRefPtr& liveStage, const OmniConnectLiveLayerRecord& layer) const;
    bool CopyLayerToDurable(const OmniConnectLiveLayerRecord& layer) const;
    bool ClearCopiedLiveLayers() const;
    template<typename CacheType> bool CopyClipsToUsd(OmniConnectActorCache& actorCache);

    bool LiveWorkflowEnabled = false;
    OmniConnectConnection* Connection = nullptr;

    UsdStageRefPtr LiveEditSceneStage;
    UsdStageRefPtr LiveSessionRootStage;
    OmniConnectLiveSessionState LiveSession;
    std::map<std::string, UsdEditTarget> PreviousEditTargets;
    // Seed each revision from the last published clip, retaining untouched arrays.
    std::map<std::string, std::string> PublishedClipUrls;
    uint64_t ClipRevision = 0;

    std::string* UsdExtension = nullptr;
    SdfPath RootPath;
    std::string LiveExtension;
};

#endif
