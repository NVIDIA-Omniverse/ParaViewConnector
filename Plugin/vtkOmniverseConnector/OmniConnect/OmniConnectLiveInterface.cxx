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

#include "OmniConnectLiveInterface.h"
#include "OmniConnectDiagnosticMgrDelegate.h"

#include <pxr/usd/sdf/copyUtils.h>
#include <pxr/usd/usdUtils/stitch.h>

#define DEBUG_DELTA_LAYERS 0

#define OmniConnectErrorMacro(x) \
  { std::stringstream logStream; \
    logStream << x; \
    std::string logString = logStream.str(); \
    OmniConnectLiveInterface::LogCallback( OmniConnectLogLevel::ERR, nullptr, logString.c_str()); } 

#define OmniConnectDebugMacro(x) \
  { std::stringstream logStream; \
    logStream << x; \
    std::string logString = logStream.str(); \
    OmniConnectLiveInterface::LogCallback( OmniConnectLogLevel::WARNING, nullptr, logString.c_str()); }  

OmniConnectLogCallback OmniConnectLiveInterface::LogCallback = nullptr;

namespace
{
  size_t BaseAssetPathLength(const std::string& assetPath, const std::string& extension)
  {
    return (assetPath.size() >= extension.size()) ? assetPath.size()-extension.size() : 0;
  }
}

void OmniConnectLiveInterface::Initialize(OmniConnectConnection* connection, OmniConnectLogCallback logCallback, std::string* usdExtension, std::string* rootPrimName)
{
  Connection = connection;
  OmniConnectLiveInterface::LogCallback = logCallback;

  UsdExtension = usdExtension;
  RootPath = SdfPath(*rootPrimName);
#if DEBUG_DELTA_LAYERS
  LiveExtension = ".usda"; // make sure to open scene with .usd extension only
#else
  LiveExtension = ".live";
#endif
}

void OmniConnectLiveInterface::SetLiveWorkflow(bool enable, UsdStageRefPtr& multiSceneStage, UsdStageRefPtr& sceneStage, 
  std::string& multiSceneStageUrl, std::string& sceneStageUrl,
  std::string& multiSceneFileName, std::string& sceneFileName,
  ActorCacheMapType* actorCaches)
{
  if(LiveWorkflowEnabled != enable)
  {
    assert(IsInitialized());

    if(LiveWorkflowEnabled)
    {
      // Merge existing live layers back into the USD layers for scene and actor stages
      if(multiSceneStage)
      {
        MergeLiveEditLayer(multiSceneStage, LiveEditMultiSceneStage);
      }

      MergeLiveEditLayer(sceneStage, LiveEditSceneStage);

      for(auto& actorCacheEntry : *actorCaches)
      {
        OmniConnectActorCache& actorCache = actorCacheEntry.second;
        MergeLiveEditLayer(actorCache.Stage, actorCache.LiveEditUsdStage);

#if DEBUG_DELTA_LAYERS == 0
        // For clip stages, copy all available .live clips to a .usd clip, adjust assetpaths
        CopyClipsToUsd<OmniConnectMeshCache>(actorCache);
        CopyClipsToUsd<OmniConnectInstancerCache>(actorCache);
        CopyClipsToUsd<OmniConnectCurveCache>(actorCache);
        CopyClipsToUsd<OmniConnectVolumeCache>(actorCache);
#endif
      }
    }
    else
    {
      // Create live edit sublayers for scene stages
      if(multiSceneStage)
      {
        LiveEditMultiSceneStage = CreateLiveEditLayer(multiSceneStage, multiSceneStageUrl, multiSceneFileName);
      }

      LiveEditSceneStage = CreateLiveEditLayer(sceneStage, sceneStageUrl, sceneFileName);
      
      // Add scene live stage as sublayer of parent multiscene live stage
      if(multiSceneStage)
        AddSubLayerToParent(LiveEditMultiSceneStage.first, LiveEditSceneStage.first);

      // Do the same for actor stages
      for(auto& actorCacheEntry : *actorCaches)
      {
        OmniConnectActorCache& actorCache = actorCacheEntry.second;
        actorCache.LiveEditUsdStage = CreateLiveEditLayer(actorCache.Stage, actorCache.UsdOutputFileUrl, actorCache.OutputFile);

        AddSubLayerToParent(LiveEditSceneStage.first, actorCache.LiveEditUsdStage.first);
      }
    }

    LiveWorkflowEnabled = enable;
  }
}

void OmniConnectLiveInterface::FilterAssetPaths(VtArray<SdfAssetPath>& assetPaths, std::string& liveAssetPath, UsdStageRefPtr& liveClipStage) const
{
  // Make sure assetPaths doesn't contain the .usd version of liveAssetPath
  if(LiveWorkflowEnabled)
  {
    assert(IsInitialized());

    size_t basePathLength = liveAssetPath.size()-LiveExtension.size(); // Live asset path, so basePathLength >= 0
    VtArray<SdfAssetPath>::iterator assetIter = assetPaths.begin();
    while(assetIter != assetPaths.end())
    {
      if(assetIter->GetAssetPath().compare(0, basePathLength, liveAssetPath, 0, basePathLength) == 0)
      {
        // Copy contents of the associated .usd layer into the .live layer
        CopyClipToLive(liveClipStage);

        // Erase the original asset path from the list
        assetIter = assetPaths.erase(assetIter);
      }
      else
        assetIter++;
    }
  }
}

bool OmniConnectLiveInterface::ModifyClipActives(VtVec2dArray& clipActives, double animTimeStep, double assetPathIdx) const
{
  assert(IsInitialized());
  
  if(LiveWorkflowEnabled)
  {
    for(GfVec2d& clipActive : clipActives)
    {
      if(clipActive[0] == animTimeStep)
      {
        clipActive[1] = assetPathIdx;
        return true;
      }
    }
  }
  return false;
}

OmniConnectLiveInterface::LiveEditStagePair OmniConnectLiveInterface::CreateLiveEditLayer(UsdStageRefPtr& stage, std::string& stageUrl, std::string& stageFileName)
{
  std::string stageName = stageFileName.substr(0, stageFileName.length()-UsdExtension->length());
  std::string liveStageName = "live_" + stageName + LiveExtension;
  std::string editTargetStageName = "deltas_" + stageName + LiveExtension;
  
  std::string liveDirUrl = stageUrl.substr(0, stageUrl.length()-stageFileName.length());
  std::string liveStageUrl = liveDirUrl + liveStageName;
  std::string editTargetUrl = liveDirUrl + editTargetStageName;

  OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(false);
  UsdStageRefPtr liveStage = UsdStage::CreateNew(liveStageUrl);
  OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(true);
  if(!liveStage)
  {
    liveStage = UsdStage::Open(liveStageUrl);
    liveStage->GetRootLayer()->GetSubLayerPaths().clear();
  }

  liveStage->SetStartTimeCode(stage->GetStartTimeCode());
  liveStage->SetEndTimeCode(stage->GetEndTimeCode());

  OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(false);
  UsdStageRefPtr editTargetStage = UsdStage::CreateNew(editTargetUrl);
  OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(true);
  if(!editTargetStage)
  {
    editTargetStage = UsdStage::Open(editTargetUrl);
    editTargetStage->RemovePrim(RootPath); // Clean up before reuse
  }

  if(liveStage && editTargetStage)
  {
    // Add the edit target to the original usd stage
    SdfLayerHandle editLayer = editTargetStage->GetRootLayer();
    stage->GetSessionLayer()->InsertSubLayerPath(editLayer->GetIdentifier());
    stage->SetEditTarget(UsdEditTarget(editLayer));

    // Add both the usd and edit target as sublayers to the live stage
    liveStage->GetRootLayer()->InsertSubLayerPath(editTargetStageName);
    liveStage->GetRootLayer()->InsertSubLayerPath(stageFileName);

    OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(false); // Generates a superfluous warning
    liveStage->Save();
    OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(true);
  }
  else
  {
    if(!liveStage)
      OmniConnectErrorMacro("Live Interface cannot create live layer at: " << liveStageUrl);
    if(!editTargetStage)
      OmniConnectErrorMacro("Live Interface cannot create edit target layer at: " << editTargetUrl);
  }

  return LiveEditStagePair(liveStage, editTargetStage);
}

void OmniConnectLiveInterface::AddSubLayerToParent(UsdStageRefPtr parentStage, UsdStageRefPtr childStage)
{
  if(!parentStage || !childStage)
    return;

  //stage->GetSessionLayer()->InsertSubLayerPath(childLayer->GetIdentifier());
  parentStage->GetRootLayer()->InsertSubLayerPath(childStage->GetRootLayer()->GetIdentifier(), 1); // Make sure child's .live layer is stronger than the parent's original .usd layer (as that indirectly references the child's .usd layer as well)
  OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(false); // Generates a superfluous warning
  parentStage->Save();
  OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(true);
}

void OmniConnectLiveInterface::MergeLiveEditLayer(UsdStageRefPtr& stage, LiveEditStagePair& liveEditStage)
{
  if(!liveEditStage.first || !liveEditStage.second)
    return;

  // retrieve the delta layer 
  // (alternative is stage->GetEditTarget().GetLayer(), but at least one stage must retain the edit layer, so existence of liveEditStage is a must)
  const SdfLayerHandle editLayer = liveEditStage.second->GetRootLayer(); 

  // Set the edit target of the stage back to the root layer
  stage->GetSessionLayer()->GetSubLayerPaths().clear();
  stage->SetEditTarget(UsdEditTarget(stage->GetRootLayer()));

  // Only merge if edits have taken place
  if(editLayer->HasSpec(RootPath)) 
  {
    const std::string& editLayerUrl = editLayer->GetIdentifier();

#if DEBUG_DELTA_LAYERS
    liveEditStage.second->Save(); // Just save the changes for debugging purposes
#else
    UsdUtilsStitchLayers(editLayer, stage->GetRootLayer()); // Merge the usd (weak) into the delta (strong) layer
    SdfCopySpec(editLayer, RootPath, stage->GetRootLayer(), RootPath); // Copy the delta layer back into the usd layer, replacing everything under the RootPath

    stage->Save();
#endif
  }

  liveEditStage = LiveEditStagePair(nullptr, nullptr); // release the live edit stage refptrs
}

void OmniConnectLiveInterface::CopyClipToLive(UsdStageRefPtr& liveClipStage) const
{
  std::string usdClipUrl = liveClipStage->GetRootLayer()->GetIdentifier();
  size_t baseUsdUrlLength = BaseAssetPathLength(usdClipUrl, LiveExtension);

  usdClipUrl = usdClipUrl.substr(0, baseUsdUrlLength) + *UsdExtension;

  // Asset found with .live extension. Copy .live stage to (newly created) .usd stage.
  OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(false);
  UsdStageRefPtr usdStage = UsdStage::Open(usdClipUrl);
  OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(true);

  if(usdStage && usdStage->GetRootLayer()->HasSpec(RootPath)) // Only copy in case live stage has contents (likely the case as it inherited the original usd clip contents)
  {
    SdfCopySpec(usdStage->GetRootLayer(), RootPath, liveClipStage->GetRootLayer(), RootPath); 
    liveClipStage->Save();
  }
}

template<typename CacheType>
void OmniConnectLiveInterface::CopyClipsToUsd(OmniConnectActorCache& actorCache)
{
  bool clipsChanged = false;
  for(auto& geomCacheEntry : actorCache.GetGeomCaches<CacheType>())
  {
    CacheType& geomCache = geomCacheEntry.second;

    bool pathChanged = false;

    for(auto assetIter = geomCache.ClipAssetPaths.begin(); 
      assetIter != geomCache.ClipAssetPaths.end(); 
      ++assetIter)
    {
      const std::string& assetStr = assetIter->GetAssetPath();
      
      // Check whether assetpath has .live extension
      size_t compareOffset = BaseAssetPathLength(assetStr, LiveExtension);
      if(assetStr.compare(compareOffset, LiveExtension.size(), LiveExtension) == 0)
      {
        // Open the live stage
        geomCache.ResetTempClipUrl(assetStr);
        const char* clipStageUrl = this->Connection->GetUrl(geomCache.TempClipUrl.c_str());
        
        UsdStageRefPtr liveStage = UsdStage::Open(clipStageUrl);

        std::string newAssetStr = assetStr.substr(0, compareOffset) + *UsdExtension;

        if(liveStage && liveStage->GetRootLayer()->HasSpec(RootPath)) // Only copy in case live stage has contents
        {
          // Asset found with .live extension. Copy .live stage to (newly created) .usd stage.
          geomCache.ResetTempClipUrl(newAssetStr);
          clipStageUrl = this->Connection->GetUrl(geomCache.TempClipUrl.c_str());
          
          OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(false); // Layer may already exist
          UsdStageRefPtr usdStage = UsdStage::CreateNew(clipStageUrl);
          OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(true);
          if(!usdStage)
            usdStage = UsdStage::Open(clipStageUrl);

          SdfCopySpec(liveStage->GetRootLayer(), RootPath, usdStage->GetRootLayer(), RootPath); 

          liveStage->RemovePrim(RootPath); // Clean up the live clip stage (cannot be done at reuse due to generic codepath).
          liveStage->Save();
          usdStage->Save();
        }

        // Change the entry into the clip asset paths
        *assetIter = SdfAssetPath(newAssetStr);
        pathChanged = true;
      }
    }

    if(pathChanged)
    {
      // If any asset path changed, push the whole array back to the geom prim in the actor layer
      UsdPrim geomPrim = actorCache.Stage->GetPrimAtPath(geomCache.SdfGeomPath);
      assert(geomPrim);

      UsdClipsAPI clipsApi(geomPrim);
      clipsApi.SetClipAssetPaths(geomCache.ClipAssetPaths);
      clipsChanged = true;
    }
  }

  if(clipsChanged)
    actorCache.Stage->Save();
}