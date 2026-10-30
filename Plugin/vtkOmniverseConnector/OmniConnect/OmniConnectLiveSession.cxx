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

#include "OmniConnectLiveSession.h"

#include <utility>

bool OmniConnectLiveSessionState::Begin(
  std::string baseLayerUrl, std::string sessionName, std::string sessionUrl)
{
  if (baseLayerUrl.empty() || sessionName.empty() || sessionUrl.empty())
    return false;

  Reset();
  BaseLayerUrl = std::move(baseLayerUrl);
  SessionName = std::move(sessionName);
  SessionUrl = std::move(sessionUrl);

  while (SessionUrl.size() > 1 && SessionUrl.back() == '/')
    SessionUrl.pop_back();

  return true;
}

void OmniConnectLiveSessionState::Reset()
{
  Layers.clear();
  BaseLayerUrl.clear();
  SessionName.clear();
  SessionUrl.clear();
}

std::string OmniConnectLiveSessionState::MakeLayerUrl(const std::string& layerName) const
{
  if (!IsActive() || layerName.empty() || layerName.front() == '/')
    return {};

    // The session API returns a namespace, not a server-side layer registry.
    return SessionUrl + "/" + layerName;
}

bool OmniConnectLiveSessionState::RegisterLayer(OmniConnectLiveLayerRecord layer)
{
  if (!IsActive() || layer.Key.empty() || layer.Name.empty())
    return false;

  if (layer.LiveUrl.empty())
    layer.LiveUrl = MakeLayerUrl(layer.Name);
  if (layer.LiveUrl.empty())
    return false;

  auto existing = Layers.find(layer.Key);
  if (existing != Layers.end())
  {
    const OmniConnectLiveLayerRecord& current = existing->second;
    return current.Name == layer.Name && current.LiveUrl == layer.LiveUrl &&
      current.DurableUrl == layer.DurableUrl && current.DurableAssetPath == layer.DurableAssetPath &&
      current.Role == layer.Role && current.MergePolicy == layer.MergePolicy && current.WriterId == layer.WriterId;
  }

  Layers.emplace(layer.Key, std::move(layer));
  return true;
}

const OmniConnectLiveLayerRecord* OmniConnectLiveSessionState::FindLayer(const std::string& key) const
{
  const auto layer = Layers.find(key);
  return layer == Layers.end() ? nullptr : &layer->second;
}

OmniConnectLiveLayerRecord* OmniConnectLiveSessionState::FindLayer(const std::string& key)
{
  const auto layer = Layers.find(key);
  return layer == Layers.end() ? nullptr : &layer->second;
}

const OmniConnectLiveLayerRecord* OmniConnectLiveSessionState::FindLayerByLiveUrl(const std::string& liveUrl) const
{
  for(const auto& layer : Layers)
  {
    if(layer.second.LiveUrl == liveUrl)
      return &layer.second;
  }
  return nullptr;
}
