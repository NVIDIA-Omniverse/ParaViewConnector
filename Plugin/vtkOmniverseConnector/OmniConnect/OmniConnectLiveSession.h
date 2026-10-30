/*###############################################################################
#
# SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#
###############################################################################*/

#ifndef OmniConnectLiveSession_h
#define OmniConnectLiveSession_h

#include <map>
#include <string>

enum class OmniConnectLiveLayerRole
{
  SessionRoot,
  ProcessAggregation,
  SceneDelta,
  ActorDelta,
  Topology,
  ValueClip
};

enum class OmniConnectLiveMergePolicy
{
  None,
  MergeDelta,
  CopyLayer
};

struct OmniConnectLiveLayerRecord
{
  std::string Key;
  std::string Name;
  std::string LiveUrl;
  std::string DurableUrl;
  std::string DurableAssetPath;
  OmniConnectLiveLayerRole Role = OmniConnectLiveLayerRole::SessionRoot;
  OmniConnectLiveMergePolicy MergePolicy = OmniConnectLiveMergePolicy::None;
  int WriterId = 0;
};

class OmniConnectLiveSessionState
{
public:
  bool Begin(std::string baseLayerUrl, std::string sessionName, std::string sessionUrl);
  void Reset();

  bool IsActive() const { return !SessionUrl.empty(); }
  const std::string& GetBaseLayerUrl() const { return BaseLayerUrl; }
  const std::string& GetSessionName() const { return SessionName; }
  const std::string& GetSessionUrl() const { return SessionUrl; }

  std::string MakeLayerUrl(const std::string& layerName) const;
  bool RegisterLayer(OmniConnectLiveLayerRecord layer);

  const OmniConnectLiveLayerRecord* FindLayer(const std::string& key) const;
  OmniConnectLiveLayerRecord* FindLayer(const std::string& key);
  const OmniConnectLiveLayerRecord* FindLayerByLiveUrl(const std::string& liveUrl) const;
  const std::map<std::string, OmniConnectLiveLayerRecord>& GetLayers() const { return Layers; }

private:
  std::string BaseLayerUrl;
  std::string SessionName;
  std::string SessionUrl;
  std::map<std::string, OmniConnectLiveLayerRecord> Layers;
};

#endif
