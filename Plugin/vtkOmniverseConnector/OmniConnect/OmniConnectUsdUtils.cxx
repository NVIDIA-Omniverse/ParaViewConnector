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

#include "OmniConnectUsdUtils.h"

TfToken TextureWrapToken(OmniConnectSamplerData::WrapMode wrapMode)
{
    TfToken result = OmniConnectTokens->black;
    switch (wrapMode)
    {
    case OmniConnectSamplerData::WrapMode::CLAMP:
      result = OmniConnectTokens->clamp;
      break;
    case OmniConnectSamplerData::WrapMode::REPEAT:
      result = OmniConnectTokens->repeat;
      break;
    case OmniConnectSamplerData::WrapMode::MIRROR:
      result = OmniConnectTokens->mirror;
      break;
    default:
      break;
    }
    return result;
}

int TextureWrapInt(OmniConnectSamplerData::WrapMode wrapMode)
{
    int result = 3;
    switch (wrapMode)
    {
    case OmniConnectSamplerData::WrapMode::CLAMP:
      result = 0;
      break;
    case OmniConnectSamplerData::WrapMode::REPEAT:
      result = 1;
      break;
    case OmniConnectSamplerData::WrapMode::MIRROR:
      result = 2;
      break;
    default:
      break;
    }
    return result;
}