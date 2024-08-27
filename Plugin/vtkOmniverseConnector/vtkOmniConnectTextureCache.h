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

#ifndef vtkOmniConnectTextureCache_h
#define vtkOmniConnectTextureCache_h

#include "OmniConnectData.h"
#include "vtkType.h"

class vtkImageData;

struct vtkOmniConnectTextureCache
{
  vtkOmniConnectTextureCache(vtkImageData* texData, bool isColorTextureMap, int texId, vtkMTimeType texMTime, OmniConnectStatusType status)
    : TexData(texData), IsColorTextureMap(isColorTextureMap), TexId(texId), TexMTime(texMTime), Status(status) {}

  void DetermineUpdateRequired();

  vtkImageData* TexData = nullptr;
  bool IsColorTextureMap = true;
  int TexId = 0;
  vtkMTimeType TexMTime = 0;
  OmniConnectStatusType Status = OmniConnectStatusType::UPDATE;
};


#endif
