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

#include "OmniConnectLiveInterface.h"
#include "OmniConnectDiagnosticMgrDelegate.h"

#include <pxr/usd/sdf/copyUtils.h>
#include <pxr/usd/sdf/fileFormat.h>
#include <pxr/usd/sdf/layerUtils.h>
#include <pxr/usd/sdf/schema.h>
#include <pxr/usd/usdUtils/stitch.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <vector>

#define OmniConnectErrorMacro(x) \
  { std::stringstream logStream; \
    logStream << x; \
    std::string logString = logStream.str(); \
    OmniConnectLiveInterface::LogCallback( OmniConnectLogLevel::ERR, nullptr, logString.c_str()); } 

OmniConnectLogCallback OmniConnectLiveInterface::LogCallback = nullptr;

namespace
{
bool EnsureLiveSubLayerOffsets(const SdfLayerHandle& layer)
{
  const size_t pathCount = layer->GetSubLayerPaths().size();
  SdfLayerOffsetVector offsets = layer->GetSubLayerOffsets();
  if(offsets.size() == pathCount)
    return true;

  // Live aggregation sublayers use identity offsets. Keep offsets aligned
  // with paths before editing either a root or a nested live layer.
  offsets.resize(pathCount);
  layer->SetField(SdfPath::AbsoluteRootPath(), SdfFieldKeys->SubLayerOffsets, offsets);
  return layer->GetSubLayerOffsets().size() == pathCount;
}
}

void OmniConnectLiveInterface::Initialize(OmniConnectConnection* connection, OmniConnectLogCallback logCallback, std::string* usdExtension, std::string* rootPrimName)
{
  Connection = connection;
  OmniConnectLiveInterface::LogCallback = logCallback;

  UsdExtension = usdExtension;
  RootPath = SdfPath(*rootPrimName);
  LiveExtension = ".live";
}

void OmniConnectLiveInterface::SetLiveWorkflow(bool enable, UsdStageRefPtr& multiSceneStage, UsdStageRefPtr& sceneStage, 
  std::string& multiSceneStageUrl, std::string& sceneStageUrl,
  std::string& multiSceneFileName, std::string& sceneFileName,
  ActorCacheMapType* actorCaches)
{
  (void)multiSceneFileName;
  (void)sceneFileName;
  UsdStageRefPtr& baseStage = multiSceneStage ? multiSceneStage : sceneStage;
  const std::string& baseLayerUrl = multiSceneStage ? multiSceneStageUrl : sceneStageUrl;

  if(LiveWorkflowEnabled != enable)
  {
    assert(IsInitialized());

    if(enable && !SdfFileFormat::FindByExtension("live"))
    {
      OmniConnectErrorMacro(
        "Cannot start live sync because the Omni USD Live file-format plugin is not registered");
      return;
    }

    if(LiveWorkflowEnabled)
    {
      bool durableCopiesSucceeded = true;
      for(auto& actorCacheEntry : *actorCaches)
      {
        OmniConnectActorCache& actorCache = actorCacheEntry.second;
        // Make durable clip assets and paths part of the actor delta before merging it.
        durableCopiesSucceeded =
          CopyClipsToUsd<OmniConnectMeshCache>(actorCache) && durableCopiesSucceeded;
        durableCopiesSucceeded =
          CopyClipsToUsd<OmniConnectInstancerCache>(actorCache) && durableCopiesSucceeded;
        durableCopiesSucceeded =
          CopyClipsToUsd<OmniConnectCurveCache>(actorCache) && durableCopiesSucceeded;
        durableCopiesSucceeded =
          CopyClipsToUsd<OmniConnectVolumeCache>(actorCache) && durableCopiesSucceeded;
      }

      if(!durableCopiesSucceeded)
      {
        OmniConnectErrorMacro(
          "Cannot end live sync because one or more live assets could not be copied to durable storage");
        return;
      }
      for(auto& actorCacheEntry : *actorCaches)
      {
        if(!MergeLiveEditLayer(actorCacheEntry.second.Stage, actorCacheEntry.second.LiveEditStage))
        {
          OmniConnectErrorMacro("Cannot end live sync because an actor delta could not be merged");
          return;
        }
      }

      UsdStageRefPtr fullSceneLiveStage = LiveEditSceneStage;
      // A previous shutdown attempt may have merged FullScene but failed during
      // later cleanup, leaving this handle null. Reopen its live layer so a
      // retry can still clear the actor sublayers below it.
      if(multiSceneStage && !fullSceneLiveStage)
      {
        const OmniConnectLiveLayerRecord* sceneLayer =
          LiveSession.FindLayer("delta:" + sceneStageUrl);
        if(sceneLayer)
          fullSceneLiveStage = UsdStage::Open(sceneLayer->LiveUrl);
        if(!fullSceneLiveStage)
        {
          OmniConnectErrorMacro("Cannot reopen the live FullScene layer for shutdown");
          return;
        }
      }
      // Normally this merges the FullScene delta and detaches its edit layer.
      if(multiSceneStage && !MergeLiveEditLayer(sceneStage, LiveEditSceneStage))
      {
        OmniConnectErrorMacro("Cannot end live sync because a scene delta could not be merged");
        return;
      }

      // root.live is also the edit layer for the base stage, not just an
      // aggregation layer. Keep the root stage handle for sublayer cleanup.
      UsdStageRefPtr baseLiveEditStage = LiveSessionRootStage;
      if(!MergeLiveEditLayer(baseStage, baseLiveEditStage))
      {
        OmniConnectErrorMacro("Cannot end live sync because the base delta could not be merged");
        return;
      }

      // Clip and topology layers can only be cleared after actor deltas have
      // stopped referencing them. Clearing them sooner forces Pcp to recompose
      // temporarily broken composition arcs during shutdown.
      if(!ClearCopiedLiveLayers())
      {
        OmniConnectErrorMacro("Cannot end live sync because copied live geometry assets could not be cleared");
        return;
      }

      // Remove child arcs only after their deltas and clip references are durable.
      if(multiSceneStage && !ClearLiveSubLayers(fullSceneLiveStage))
      {
        OmniConnectErrorMacro("Cannot clear the live FullScene sublayers");
        return;
      }
      if(!ClearLiveSubLayers(LiveSessionRootStage))
      {
        OmniConnectErrorMacro("Cannot clear the live session root sublayers");
        return;
      }
      LiveSessionRootStage = nullptr;
      StopLiveSession();
    }
    else
    {
      if(!StartLiveSession(baseLayerUrl))
        return;

      LiveSessionRootStage = CreateSessionRootLayer();
      if(!LiveSessionRootStage)
      {
        StopLiveSession();
        return;
      }
      // The session root edits whichever durable stage is the session base.
      bool activationSucceeded = AttachLiveEditLayer(baseStage, LiveSessionRootStage);
      UsdStageRefPtr fullSceneLiveStage = LiveSessionRootStage;
      if(multiSceneStage)
      {
        // Only this process's FullScene is represented under MultiScene for now.
        LiveEditSceneStage = CreateLiveEditLayer(
          sceneStage, sceneStageUrl, "scene", OmniConnectLiveLayerRole::SceneDelta);
        activationSucceeded = activationSucceeded && LiveEditSceneStage;
        activationSucceeded = AddSubLayerToParent(LiveSessionRootStage, LiveEditSceneStage)
          && activationSucceeded;
        fullSceneLiveStage = LiveEditSceneStage;
      }

      for(auto& actorCacheEntry : *actorCaches)
      {
        OmniConnectActorCache& actorCache = actorCacheEntry.second;
        actorCache.LiveEditStage = CreateLiveEditLayer(
          actorCache.Stage, actorCache.UsdOutputFileUrl, "actor", OmniConnectLiveLayerRole::ActorDelta);
        activationSucceeded = activationSucceeded && actorCache.LiveEditStage;
        activationSucceeded = AddSubLayerToParent(fullSceneLiveStage, actorCache.LiveEditStage)
          && activationSucceeded;
      }

      if(sceneStage && fullSceneLiveStage &&
        fullSceneLiveStage->GetRootLayer()->GetSubLayerPaths().size() !=
          sceneStage->GetRootLayer()->GetSubLayerPaths().size())
      {
        // Each durable actor needs a reachable live counterpart in this process.
        OmniConnectErrorMacro(
          "Cannot activate live sync because the live FullScene layer's actor sublayer count does not "
          "match FullScene's durable sublayer count");
        activationSucceeded = false;
      }

      if(!activationSucceeded)
      {
        // Undo stage edit targets and composition arcs before leaving the session.
        for(auto& actorCacheEntry : *actorCaches)
          DetachLiveEditLayer(actorCacheEntry.second.Stage, actorCacheEntry.second.LiveEditStage);
        if(multiSceneStage)
          DetachLiveEditLayer(sceneStage, LiveEditSceneStage);
        UsdStageRefPtr baseLiveEditStage = LiveSessionRootStage;
        DetachLiveEditLayer(baseStage, baseLiveEditStage);

        if(multiSceneStage && !ClearLiveSubLayers(fullSceneLiveStage))
          OmniConnectErrorMacro("Cannot clear live FullScene sublayers after failed activation");
        if(!ClearLiveSubLayers(LiveSessionRootStage))
          OmniConnectErrorMacro("Cannot clear live session root sublayers after failed activation");
        LiveSessionRootStage = nullptr;
        StopLiveSession();
        return;
      }

      // Creating and editing a live layer only queues the changes locally. Pump the
      // client once after the complete root hierarchy has been authored so clients
      // joining the session can compose root.live immediately.
      Connection->ProcessUpdates();
    }

    LiveWorkflowEnabled = enable;
  }
}

bool OmniConnectLiveInterface::StartLiveSession(const std::string& baseLayerUrl)
{
  static const std::string sessionName = "ParaView";

  OmniConnectLiveSessionInfoList sessions;
  if(!Connection->ListLiveSessions(baseLayerUrl.c_str(), sessions))
    return false;

  // Sessions belong to the base stage; reuse its named session if it exists.
  std::string sessionUrl;
  const auto existingSession = std::find_if(sessions.begin(), sessions.end(),
    [](const OmniConnectLiveSessionInfo& session) { return session.Name == "ParaView"; });
  if(existingSession != sessions.end())
    sessionUrl = existingSession->Url;
  else if(!Connection->CreateLiveSession(baseLayerUrl.c_str(), sessionName.c_str(), sessionUrl))
    return false;

  if(!Connection->JoinLiveSession(sessionUrl.c_str(), "ParaView Connector"))
    return false;

  if(!LiveSession.Begin(baseLayerUrl, sessionName, sessionUrl))
  {
    Connection->LeaveLiveSession();
    return false;
  }

  return true;
}

void OmniConnectLiveInterface::StopLiveSession()
{
  Connection->LeaveLiveSession();
  LiveSession.Reset();
  PreviousEditTargets.clear();
  PublishedClipUrls.clear();
}

UsdStageRefPtr OmniConnectLiveInterface::CreateSessionRootLayer()
{
  // Kit opens this conventional name relative to the opaque session URL.
  // The session API does not otherwise designate a USD root layer.
  OmniConnectLiveLayerRecord rootLayer;
  rootLayer.Key = "session-root";
  rootLayer.Name = "root.live";
  rootLayer.Role = OmniConnectLiveLayerRole::SessionRoot;
  rootLayer.MergePolicy = OmniConnectLiveMergePolicy::None;
  if(!LiveSession.RegisterLayer(rootLayer))
    return nullptr;

  const OmniConnectLiveLayerRecord* registeredRoot = LiveSession.FindLayer(rootLayer.Key);
  if(!registeredRoot)
    return nullptr;

  OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(false);
  UsdStageRefPtr rootStage = UsdStage::CreateNew(registeredRoot->LiveUrl);
  OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(true);
  // An earlier run may have left this named layer in the reusable session.
  if(!rootStage)
    rootStage = UsdStage::Open(registeredRoot->LiveUrl);
  if(!rootStage)
  {
    OmniConnectErrorMacro("Cannot create the live session root layer at: " << registeredRoot->LiveUrl);
    return nullptr;
  }

  return rootStage;
}

void OmniConnectLiveInterface::ResetTopologyFile(
  const OmniConnectActorCache& actorCache, OmniConnectGeomCache& geomCache)
{
  geomCache.ResetTopologyFile(actorCache, *UsdExtension);
  if(!LiveWorkflowEnabled)
    return;

  const std::string durableAssetPath = geomCache.SceneRelGeomTopologyFile;
  const std::string durablePath = geomCache.GeomTopologyStagePath;
  const std::string durableUrl = Connection->GetUrl(durablePath.c_str());

  // Keep the durable path for shutdown while routing actor references to live.
  OmniConnectLiveLayerRecord layer;
  layer.Key = "topology:" + durableUrl;
  layer.Name = MakeLiveLayerName("topology", durableUrl);
  layer.DurableUrl = durableUrl;
  layer.DurableAssetPath = durableAssetPath;
  layer.Role = OmniConnectLiveLayerRole::Topology;
  layer.MergePolicy = OmniConnectLiveMergePolicy::CopyLayer;
  if(!LiveSession.RegisterLayer(layer))
  {
    OmniConnectErrorMacro("Cannot register live topology layer for: " << durableUrl);
    return;
  }

  const OmniConnectLiveLayerRecord* registeredLayer = LiveSession.FindLayer(layer.Key);
  assert(registeredLayer);
  geomCache.SceneRelGeomTopologyFile = registeredLayer->LiveUrl;
  geomCache.GeomTopologyStagePath = registeredLayer->LiveUrl;
}

void OmniConnectLiveInterface::ResetTimedGeomClipFile(
  const OmniConnectActorCache& actorCache, OmniConnectGeomCache& geomCache,
  double animTimeStep, bool createRevision)
{
  (void)actorCache;
  geomCache.ResetTimedGeomClipFile(animTimeStep, *UsdExtension);
  if(!LiveWorkflowEnabled)
    return;

  const std::string durableAssetPath = geomCache.TimedSceneRelGeomClipFile;
  const std::string durablePath = geomCache.TimedGeomClipStagePath;
  const std::string durableUrl = Connection->GetUrl(durablePath.c_str());

  if(!createRevision)
  {
    const auto previous = PublishedClipUrls.find(durableUrl);
    if(previous != PublishedClipUrls.end())
    {
      geomCache.TimedSceneRelGeomClipFile = previous->second;
      geomCache.TimedGeomClipStagePath = previous->second;
      PublishedClipUrls.erase(previous);
    }
    return;
  }

  // WORKAROUND: repeated topology edits to an existing live value clip can
  // leave Kit's rendered points stale even when its composed USD arrays are
  // correct. This was reproduced with both Nucleus and Storage API sessions.
  // A fresh clip identifier forces a consuming-stage resync; reauthoring the
  // same clip metadata does not. Keep this until in-place clip edits reliably
  // invalidate the consumer's geometry, then retest both session backends.
  // Superseded revisions remain registered for cleanup at session shutdown.
  // Include a clock value so reused sessions cannot expose an earlier run's clip.
  const std::string revision = std::to_string(
    std::chrono::high_resolution_clock::now().time_since_epoch().count()) +
    "_" + std::to_string(++ClipRevision);
  OmniConnectLiveLayerRecord layer;
  layer.Key = "clip:" + durableUrl + ":" + revision;
  layer.Name = MakeLiveLayerName("clip_r" + revision, durableUrl);
  layer.DurableUrl = durableUrl;
  layer.DurableAssetPath = durableAssetPath;
  layer.Role = OmniConnectLiveLayerRole::ValueClip;
  layer.MergePolicy = OmniConnectLiveMergePolicy::CopyLayer;
  if(!LiveSession.RegisterLayer(layer))
  {
    OmniConnectErrorMacro("Cannot register live value clip layer for: " << durableUrl);
    return;
  }

  const OmniConnectLiveLayerRecord* registeredLayer = LiveSession.FindLayer(layer.Key);
  assert(registeredLayer);
  geomCache.TimedSceneRelGeomClipFile = registeredLayer->LiveUrl;
  geomCache.TimedGeomClipStagePath = registeredLayer->LiveUrl;
}

void OmniConnectLiveInterface::InitializeLiveLayer(UsdStageRefPtr& liveStage) const
{
  if(!LiveWorkflowEnabled || !liveStage)
    return;

  const OmniConnectLiveLayerRecord* layer =
    LiveSession.FindLayerByLiveUrl(liveStage->GetRootLayer()->GetIdentifier());
  if(layer)
  {
    OmniConnectLiveLayerRecord seed = *layer;
    const auto previous = PublishedClipUrls.find(layer->DurableUrl);
    if(layer->Role == OmniConnectLiveLayerRole::ValueClip &&
      previous != PublishedClipUrls.end())
      seed.DurableUrl = previous->second;
    CopyLayerToLive(liveStage, seed);
  }
}

bool OmniConnectLiveInterface::PublishLiveClip(OmniConnectActorCache& actorCache,
  OmniConnectGeomCache& geomCache, double animTimeStep)
{
  if(!LiveWorkflowEnabled)
    return true;

  const OmniConnectLiveLayerRecord* layer =
    LiveSession.FindLayerByLiveUrl(geomCache.TimedSceneRelGeomClipFile);
  const SdfLayerRefPtr clipLayer = layer ? SdfLayer::FindOrOpen(layer->LiveUrl) : nullptr;
  // Publish the fresh identifier required by the live-clip invalidation workaround.
  // Do not advertise the revision until all of its arrays have been authored
  // and saved. This is ordered publication, not a cross-layer transaction.
  if(!clipLayer || !clipLayer->HasSpec(RootPath) || !clipLayer->Save())
  {
    OmniConnectErrorMacro("Cannot publish live clip revision: "
      << geomCache.TimedSceneRelGeomClipFile);
    return false;
  }
  Connection->ProcessUpdates();

  UsdPrim geomPrim = actorCache.Stage->GetPrimAtPath(geomCache.SdfGeomPath);
  if(!geomPrim)
    return false;
  UsdClipsAPI clipsApi(geomPrim);
  bool pathsChanged = false;
  const size_t index = UpdateClipAssetPath(
    geomCache.ClipAssetPaths, layer->LiveUrl, pathsChanged);
  bool activeChanged = false;
  if(!ModifyClipActives(geomCache.ClipActives, animTimeStep,
      static_cast<double>(index), activeChanged))
  {
    geomCache.ClipActives.push_back(GfVec2d(animTimeStep, static_cast<double>(index)));
    activeChanged = true;
  }
  if((pathsChanged && !clipsApi.SetClipAssetPaths(geomCache.ClipAssetPaths)) ||
    (activeChanged && !clipsApi.SetClipActive(geomCache.ClipActives)))
  {
    OmniConnectErrorMacro("Cannot author live clip revision metadata for: " << geomCache.SdfGeomPath);
    return false;
  }
  PublishedClipUrls[layer->DurableUrl] = layer->LiveUrl;
  return true;
}

SdfAssetPath OmniConnectLiveInterface::GetAssetPathForAuthoring(
  const UsdStageRefPtr& durableStage, const SdfAssetPath& durableAssetPath) const
{
  if(!LiveWorkflowEnabled || !durableStage || durableAssetPath.GetAssetPath().empty())
    return durableAssetPath;

  // A relative path authored in live:// would resolve from the session URL.
  // Anchor file dependencies to their durable layer instead.
  return SdfAssetPath(SdfComputeAssetPathRelativeToLayer(
    durableStage->GetRootLayer(), durableAssetPath.GetAssetPath()));
}

void OmniConnectLiveInterface::FlushStage(UsdStageRefPtr& stage)
{
  if(!stage)
    return;

  if(!LiveWorkflowEnabled)
  {
    stage->Save();
    return;
  }

  const SdfLayerHandle editLayer = stage->GetEditTarget().GetLayer();
  const OmniConnectLiveLayerRecord* layer = editLayer ?
    LiveSession.FindLayerByLiveUrl(editLayer->GetIdentifier()) : nullptr;
  if(!layer)
  {
    OmniConnectErrorMacro("Cannot flush stage because its edit target is not a registered live layer");
    return;
  }

  if(!editLayer->Save())
  {
    OmniConnectErrorMacro("Cannot save live layer: " << layer->LiveUrl);
    return;
  }

  // Live edits are queued by the .live layer state delegate. This call sends
  // local updates and applies queued remote updates on the USD thread.
  Connection->ProcessUpdates();
}

void OmniConnectLiveInterface::FlushActorStage(OmniConnectActorCache& actorCache)
{
  if(!LiveWorkflowEnabled)
  {
    FlushStage(actorCache.Stage);
    return;
  }

  if(!actorCache.Stage || !actorCache.LiveEditStage)
  {
    OmniConnectErrorMacro(
      "Cannot flush live actor stage for " << actorCache.SdfActorPrimPath
      << " because its live edit stage is not available");
    return;
  }

  const SdfLayerHandle liveLayer = actorCache.LiveEditStage->GetRootLayer();
  const SdfLayerHandle editLayer = actorCache.Stage->GetEditTarget().GetLayer();
  // Saving the wrong target would leave actor edits out of the live session.
  if(!editLayer || editLayer->GetIdentifier() != liveLayer->GetIdentifier())
  {
    OmniConnectErrorMacro(
      "Cannot flush live actor stage for " << actorCache.SdfActorPrimPath
      << " because its edit target is "
      << (editLayer ? editLayer->GetIdentifier() : std::string("<none>"))
      << " instead of " << liveLayer->GetIdentifier());
    return;
  }

  FlushStage(actorCache.LiveEditStage);
}

size_t OmniConnectLiveInterface::UpdateClipAssetPath(VtArray<SdfAssetPath>& assetPaths,
  const std::string& assetPath, bool& changed) const
{
  changed = false;
  if(LiveWorkflowEnabled)
  {
    assert(IsInitialized());

    const OmniConnectLiveLayerRecord* layer = LiveSession.FindLayerByLiveUrl(assetPath);
    if(layer)
    {
      for(size_t assetIndex = 0; assetIndex < assetPaths.size(); ++assetIndex)
      {
        const std::string& existingPath = assetPaths[assetIndex].GetAssetPath();
        if(existingPath == layer->LiveUrl)
          return assetIndex;
        const OmniConnectLiveLayerRecord* existingLayer =
          LiveSession.FindLayerByLiveUrl(existingPath);
        if(existingPath == layer->DurableAssetPath || existingPath == layer->DurableUrl ||
          (existingLayer && existingLayer->Role == OmniConnectLiveLayerRole::ValueClip &&
            existingLayer->DurableUrl == layer->DurableUrl))
        {
          // Preserve the existing clip index when redirecting it to live.
          assetPaths[assetIndex] = SdfAssetPath(layer->LiveUrl);
          changed = true;
          return assetIndex;
        }
      }
    }
  }

  assetPaths.push_back(SdfAssetPath(assetPath));
  changed = true;
  return assetPaths.size() - 1;
}

bool OmniConnectLiveInterface::ModifyClipActives(VtVec2dArray& clipActives,
  double animTimeStep, double assetPathIdx, bool& changed) const
{
  assert(IsInitialized());
  changed = false;
  
  if(LiveWorkflowEnabled)
  {
    for(GfVec2d& clipActive : clipActives)
    {
      if(clipActive[0] == animTimeStep)
      {
        // Existing timesteps keep their entry; only redirect a changed index.
        if(clipActive[1] != assetPathIdx)
        {
          clipActive[1] = assetPathIdx;
          changed = true;
        }
        return true;
      }
    }
  }
  return false;
}

std::string OmniConnectLiveInterface::MakeLiveLayerName(
  const std::string& prefix, const std::string& stableIdentity) const
{
  // Stable names let a reused session find the same layer for a durable URL.
  uint64_t hash = 14695981039346656037ull;
  for(const unsigned char character : stableIdentity)
  {
    hash ^= character;
    hash *= 1099511628211ull;
  }

  std::ostringstream layerName;
  layerName << prefix << "_" << std::hex << std::setfill('0') << std::setw(16) << hash << LiveExtension;
  return layerName.str();
}

UsdStageRefPtr OmniConnectLiveInterface::CreateLiveEditLayer(
  UsdStageRefPtr& stage, const std::string& durableLayerUrl,
  const std::string& layerNamePrefix, OmniConnectLiveLayerRole role)
{
  if(!stage || !LiveSession.IsActive())
    return nullptr;

  OmniConnectLiveLayerRecord layer;
  layer.Key = "delta:" + durableLayerUrl;
  layer.Name = MakeLiveLayerName(layerNamePrefix, durableLayerUrl);
  layer.DurableUrl = durableLayerUrl;
  layer.Role = role;
  layer.MergePolicy = OmniConnectLiveMergePolicy::MergeDelta;
  if(!LiveSession.RegisterLayer(layer))
    return nullptr;

  const OmniConnectLiveLayerRecord* registeredLayer = LiveSession.FindLayer(layer.Key);
  if(!registeredLayer)
    return nullptr;

  OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(false);
  UsdStageRefPtr editTargetStage = UsdStage::CreateNew(registeredLayer->LiveUrl);
  OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(true);
  // Reopen the existing layer when this named session is reused.
  if(!editTargetStage)
    editTargetStage = UsdStage::Open(registeredLayer->LiveUrl);

  if(editTargetStage && !AttachLiveEditLayer(stage, editTargetStage))
    return nullptr;
  if(!editTargetStage)
  {
    OmniConnectErrorMacro("Cannot create live edit layer at: " << registeredLayer->LiveUrl);
  }

  return editTargetStage;
}

bool OmniConnectLiveInterface::AttachLiveEditLayer(
  UsdStageRefPtr& stage, const UsdStageRefPtr& liveEditStage)
{
  if(!stage || !liveEditStage)
    return false;

  const SdfLayerHandle editLayer = liveEditStage->GetRootLayer();
  const std::string stageIdentifier = stage->GetRootLayer()->GetIdentifier();
  PreviousEditTargets.emplace(stageIdentifier, stage->GetEditTarget());

  // The session-layer arc chooses where this stage writes; the live parent
  // sublayer arc separately makes the same edits visible from root.live.
  SdfSubLayerProxy sessionSubLayers = stage->GetSessionLayer()->GetSubLayerPaths();
  if(sessionSubLayers.Find(editLayer->GetIdentifier()) == -1)
    stage->GetSessionLayer()->InsertSubLayerPath(editLayer->GetIdentifier(), 0);
  stage->SetEditTarget(UsdEditTarget(editLayer));
  return true;
}

bool OmniConnectLiveInterface::AddSubLayerToParent(UsdStageRefPtr parentStage, UsdStageRefPtr childStage)
{
  if(!parentStage || !childStage)
    return false;

  const SdfLayerHandle parentLayer = parentStage->GetRootLayer();
  if(!EnsureLiveSubLayerOffsets(parentLayer))
  {
    OmniConnectErrorMacro("Cannot repair live sublayer offsets before adding a layer: "
      << parentLayer->GetIdentifier());
    return false;
  }

  SdfSubLayerProxy subLayers = parentLayer->GetSubLayerPaths();
  const std::string childIdentifier = childStage->GetRootLayer()->GetIdentifier();
  // Do not duplicate an arc when reopening an existing live hierarchy.
  if(subLayers.Find(childIdentifier) == -1)
    parentLayer->InsertSubLayerPath(childIdentifier);
  if(!EnsureLiveSubLayerOffsets(parentLayer))
  {
    OmniConnectErrorMacro("Cannot repair live sublayer offsets after adding a layer: "
      << parentLayer->GetIdentifier());
    return false;
  }
  OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(false); // Generates a superfluous warning
  const bool saved = parentLayer->Save();
  OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(true);
  if(!saved)
    OmniConnectErrorMacro("Cannot save live sublayer parent: " << parentLayer->GetIdentifier());
  return saved;
}

bool OmniConnectLiveInterface::ClearLiveSubLayers(const UsdStageRefPtr& stage)
{
  if(!stage)
    return true;

  const SdfLayerHandle layer = stage->GetRootLayer();
  // Offset repair must precede path edits; USD expects parallel arrays.
  if(!EnsureLiveSubLayerOffsets(layer))
    return false;

  // Retain both arrays so a failed save can restore the in-memory hierarchy.
  const SdfSubLayerProxy subLayers = layer->GetSubLayerPaths();
  const std::vector<std::string> previousPaths(subLayers.begin(), subLayers.end());
  const SdfLayerOffsetVector previousOffsets = layer->GetSubLayerOffsets();
  // Even without child paths, save any offset repair made above.
  if(previousPaths.empty())
    return layer->Save();

  // Clear the composition arcs, not the child layers or their stored data.
  layer->GetSubLayerPaths().clear();
  if(layer->Save())
    return true;

  // Restore paths and their matching offsets if the cleared state did not save.
  for(const std::string& path : previousPaths)
    layer->InsertSubLayerPath(path);
  layer->SetField(SdfPath::AbsoluteRootPath(), SdfFieldKeys->SubLayerOffsets, previousOffsets);
  OmniConnectErrorMacro("Cannot save cleared live sublayers: " << layer->GetIdentifier());
  return false;
}

void OmniConnectLiveInterface::DetachLiveEditLayer(UsdStageRefPtr& stage, UsdStageRefPtr& liveEditStage)
{
  if(!stage || !liveEditStage)
    return;

  const std::string editLayerIdentifier = liveEditStage->GetRootLayer()->GetIdentifier();
  const std::string stageIdentifier = stage->GetRootLayer()->GetIdentifier();
  const auto previousEditTarget = PreviousEditTargets.find(stageIdentifier);
  if(previousEditTarget != PreviousEditTargets.end())
  {
    stage->SetEditTarget(previousEditTarget->second);
    PreviousEditTargets.erase(previousEditTarget);
  }
  else
    stage->SetEditTarget(UsdEditTarget(stage->GetRootLayer()));

  // Restore a valid edit target before removing the live layer from the stage's
  // layer stack. Removing the current edit target triggers recomposition while
  // its Pcp layer-stack entry is no longer available.
  SdfSubLayerProxy sessionSubLayers = stage->GetSessionLayer()->GetSubLayerPaths();
  const size_t sessionLayerIndex = sessionSubLayers.Find(editLayerIdentifier);
  if(sessionLayerIndex != size_t(-1))
    stage->GetSessionLayer()->RemoveSubLayerPath(static_cast<int>(sessionLayerIndex));

  liveEditStage = nullptr;
}

bool OmniConnectLiveInterface::MergeLiveEditLayer(UsdStageRefPtr& stage, UsdStageRefPtr& liveEditStage)
{
  if(!liveEditStage)
    return true;
  if(!stage)
  {
    OmniConnectErrorMacro("Cannot merge live delta without a durable stage");
    return false;
  }

  // Retain the live stage locally: detaching below clears the caller's handle.
  const UsdStageRefPtr editStage = liveEditStage;
  const SdfLayerHandle editLayer = liveEditStage->GetRootLayer();

  // Only merge if edits have taken place
  if(!editLayer->HasSpec(RootPath))
  {
    DetachLiveEditLayer(stage, liveEditStage);
    return true;
  }

  // Work on a detached layer; retain both originals for in-memory rollback
  // if applying or clearing the merge fails.
  SdfLayerRefPtr liveBackup = SdfLayer::CreateAnonymous(".usda");
  SdfLayerRefPtr mergedLayer = SdfLayer::CreateAnonymous(".usda");
  const SdfLayerHandle durableLayer = stage->GetRootLayer();
  const bool hadDurableSpec = durableLayer->HasSpec(RootPath);
  SdfLayerRefPtr durableBackup = SdfLayer::CreateAnonymous(".usda");
  if(!liveBackup || !mergedLayer || !durableBackup ||
    !SdfCopySpec(editLayer, RootPath, liveBackup, RootPath) ||
    !SdfCopySpec(editLayer, RootPath, mergedLayer, RootPath) ||
    (hadDurableSpec && !SdfCopySpec(durableLayer, RootPath, durableBackup, RootPath)))
  {
    OmniConnectErrorMacro("Cannot prepare live delta for merge: " << editLayer->GetIdentifier());
    return false;
  }

  UsdUtilsStitchLayers(mergedLayer, durableLayer);
  // Detach before changing durable specs so the live layer cannot mask them.
  DetachLiveEditLayer(stage, liveEditStage);

  if(!SdfCopySpec(mergedLayer, RootPath, durableLayer, RootPath) || !durableLayer->Save())
  {
    // Restore the prior durable spec and reattach live editing for a retry.
    const bool restored = hadDurableSpec ?
      SdfCopySpec(durableBackup, RootPath, durableLayer, RootPath) : stage->RemovePrim(RootPath);
    if(!restored)
      OmniConnectErrorMacro("Cannot restore durable layer after a failed live merge: "
        << durableLayer->GetIdentifier());
    liveEditStage = editStage;
    AttachLiveEditLayer(stage, liveEditStage);
    OmniConnectErrorMacro("Cannot save merged live delta to durable layer: "
      << durableLayer->GetIdentifier());
    return false;
  }

  // Clear the merged /Root spec, not the live layer itself. The named session
  // may be reused, so stale opinions must not remain in it.
  if(!editStage->RemovePrim(RootPath) || !editLayer->Save())
  {
    if(!editLayer->HasSpec(RootPath) &&
      !SdfCopySpec(liveBackup, RootPath, editLayer, RootPath))
      OmniConnectErrorMacro("Cannot restore live delta after a failed clear: "
        << editLayer->GetIdentifier());
    liveEditStage = editStage;
    AttachLiveEditLayer(stage, liveEditStage);
    OmniConnectErrorMacro("Cannot clear merged live delta: " << editLayer->GetIdentifier());
    return false;
  }
  return true;
}

void OmniConnectLiveInterface::CopyLayerToLive(
  UsdStageRefPtr& liveStage, const OmniConnectLiveLayerRecord& layer) const
{
  // Existing clips/topologies need their durable data once; later edits use
  // the same live layer without recopying its /Root subtree.
  if(!liveStage || liveStage->GetRootLayer()->HasSpec(RootPath) || layer.DurableUrl.empty())
    return;

  OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(false);
  UsdStageRefPtr durableStage = UsdStage::Open(layer.DurableUrl);
  OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(true);

  if(durableStage && durableStage->GetRootLayer()->HasSpec(RootPath))
  {
    SdfCopySpec(durableStage->GetRootLayer(), RootPath, liveStage->GetRootLayer(), RootPath);
    liveStage->Save();
  }
}

bool OmniConnectLiveInterface::CopyLayerToDurable(const OmniConnectLiveLayerRecord& layer) const
{
  UsdStageRefPtr liveStage = UsdStage::Open(layer.LiveUrl);
  if(!liveStage || !liveStage->GetRootLayer()->HasSpec(RootPath))
    return true;

  OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(false);
  UsdStageRefPtr durableStage = UsdStage::CreateNew(layer.DurableUrl);
  OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(true);
  // A touched timestep may already have a durable clip from an earlier save.
  if(!durableStage)
    durableStage = UsdStage::Open(layer.DurableUrl);
  if(!durableStage)
    return false;

  if(!SdfCopySpec(liveStage->GetRootLayer(), RootPath, durableStage->GetRootLayer(), RootPath) ||
    !durableStage->GetRootLayer()->Save())
    return false;
  return true;
}

bool OmniConnectLiveInterface::ClearCopiedLiveLayers() const
{
  for(const auto& layerEntry : LiveSession.GetLayers())
  {
    const OmniConnectLiveLayerRecord& layer = layerEntry.second;
    if(layer.MergePolicy != OmniConnectLiveMergePolicy::CopyLayer)
      continue;

    UsdStageRefPtr liveStage = UsdStage::Open(layer.LiveUrl);
    if(!liveStage || !liveStage->GetRootLayer()->HasSpec(RootPath))
      continue;

    SdfLayerRefPtr liveBackup = SdfLayer::CreateAnonymous(".usda");
    // Keep the copied clip/topology spec available if its clear cannot save.
    if(!liveBackup || !SdfCopySpec(liveStage->GetRootLayer(), RootPath, liveBackup, RootPath))
    {
      OmniConnectErrorMacro("Cannot prepare copied live layer for clearing: " << layer.LiveUrl);
      return false;
    }
    if(!liveStage->RemovePrim(RootPath) || !liveStage->GetRootLayer()->Save())
    {
      if(!liveStage->GetRootLayer()->HasSpec(RootPath) &&
        !SdfCopySpec(liveBackup, RootPath, liveStage->GetRootLayer(), RootPath))
        OmniConnectErrorMacro("Cannot restore copied live layer after a failed clear: "
          << layer.LiveUrl);
      OmniConnectErrorMacro("Cannot clear copied live layer: " << layer.LiveUrl);
      return false;
    }
  }
  return true;
}

template<typename CacheType>
bool OmniConnectLiveInterface::CopyClipsToUsd(OmniConnectActorCache& actorCache)
{
  bool copySucceeded = true;
  bool clipsChanged = false;
  for(auto& geomCacheEntry : actorCache.GetGeomCaches<CacheType>())
  {
    CacheType& geomCache = geomCacheEntry.second;
    bool pathChanged = false;

    for(SdfAssetPath& assetPath : geomCache.ClipAssetPaths)
    {
      const OmniConnectLiveLayerRecord* layer =
        LiveSession.FindLayerByLiveUrl(assetPath.GetAssetPath());
      if(layer && layer->Role == OmniConnectLiveLayerRole::ValueClip)
      {
        if(!CopyLayerToDurable(*layer))
        {
          OmniConnectErrorMacro("Cannot copy live value clip to durable layer: " << layer->DurableUrl);
          copySucceeded = false;
          continue;
        }
        // The actor delta must point back to the durable clip before merging.
        assetPath = SdfAssetPath(layer->DurableAssetPath);
        pathChanged = true;
      }
    }

    UsdPrim geomPrim = actorCache.Stage->GetPrimAtPath(geomCache.SdfGeomPath);
    if(!geomPrim)
      continue;

    UsdClipsAPI clipsApi(geomPrim);
    if(pathChanged)
    {
      clipsApi.SetClipAssetPaths(geomCache.ClipAssetPaths);
      clipsChanged = true;
    }

    const OmniConnectLiveLayerRecord* topologyLayer =
      LiveSession.FindLayerByLiveUrl(geomCache.SceneRelGeomTopologyFile);
    if(topologyLayer && topologyLayer->Role == OmniConnectLiveLayerRole::Topology)
    {
      if(!CopyLayerToDurable(*topologyLayer))
      {
        OmniConnectErrorMacro("Cannot copy live topology to durable layer: " << topologyLayer->DurableUrl);
        copySucceeded = false;
        continue;
      }

      // Restore both the value-clip manifest and the geometry reference.
      clipsApi.SetClipManifestAssetPath(SdfAssetPath(topologyLayer->DurableAssetPath));
#ifndef TOPOLOGY_AS_SUBLAYER
      geomPrim.GetReferences().ClearReferences();
      geomPrim.GetReferences().AddReference(topologyLayer->DurableAssetPath, geomCache.SdfGeomPath);
#endif
      geomCache.SceneRelGeomTopologyFile = topologyLayer->DurableAssetPath;
      geomCache.GeomTopologyStagePath = topologyLayer->DurableUrl;
      clipsChanged = true;
    }
  }

  if(clipsChanged)
    FlushStage(actorCache.Stage);

  return copySucceeded;
}
