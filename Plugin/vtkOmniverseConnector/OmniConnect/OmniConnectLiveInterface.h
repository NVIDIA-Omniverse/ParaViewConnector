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

#ifndef OmniConnectLiveInterface_h
#define OmniConnectLiveInterface_h

#include "usd.h"

PXR_NAMESPACE_USING_DIRECTIVE

#include "OmniConnectCaches.h"
#include "OmniConnectConnection.h"

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

    // Filter usd asset paths matching a live asset path. Sideeffect: if a match is found, it's contents are copied over to liveClipStage.
    void FilterAssetPaths(VtArray<SdfAssetPath>& assetPaths, std::string& liveAssetPath, UsdStageRefPtr& liveClipStage) const; 
    bool ModifyClipActives(VtVec2dArray& clipActives, double animTimeStep, double assetPathIdx) const;
    
    const std::string& GetLiveExtension() const { return LiveWorkflowEnabled ? LiveExtension : *UsdExtension; }

    static OmniConnectLogCallback LogCallback;

  protected:
    using LiveEditStagePair = std::pair<UsdStageRefPtr, UsdStageRefPtr>;

    LiveEditStagePair CreateLiveEditLayer(UsdStageRefPtr& stage, std::string& stageUrl, std::string& stageFileName);
    void AddSubLayerToParent(UsdStageRefPtr parentStage, UsdStageRefPtr childStage);

    void MergeLiveEditLayer(UsdStageRefPtr& stage, LiveEditStagePair& liveEditStage);
    void CopyClipToLive(UsdStageRefPtr& liveClipStage) const;
    template<typename CacheType> void CopyClipsToUsd(OmniConnectActorCache& actorCache);

    bool LiveWorkflowEnabled = false;
    OmniConnectConnection* Connection = nullptr;

    LiveEditStagePair LiveEditMultiSceneStage;
    LiveEditStagePair LiveEditSceneStage;

    std::string* UsdExtension = nullptr;
    SdfPath RootPath;
    std::string LiveExtension;
};

#endif