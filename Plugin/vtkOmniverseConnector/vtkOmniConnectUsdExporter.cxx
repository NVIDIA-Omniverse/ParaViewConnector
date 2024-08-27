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

#include "vtkOmniConnectUsdExporter.h"

#include "vtkObjectFactory.h"
#include "vtkRenderer.h"
#include "vtkOmniConnectPass.h"
#include "vtkOmniConnectRendererNode.h"
#include "vtkRenderState.h"
#include "OmniConnectUtilsExternal.h"

#include <cstring>

vtkStandardNewMacro(vtkOmniConnectUsdExporter);

// ----------------------------------------------------------------------------
vtkOmniConnectUsdExporter::vtkOmniConnectUsdExporter()
{
  this->FileName = nullptr;
}

// ----------------------------------------------------------------------------
vtkOmniConnectUsdExporter::~vtkOmniConnectUsdExporter()
{
  delete[] this->FileName;
}

// ----------------------------------------------------------------------------
void vtkOmniConnectUsdExporter::WriteData()
{
  vtkRenderer* ren = GetActiveRenderer();

  if(ren)
  {
    vtkNew<vtkOmniConnectPass> omniConnectPass;

    vtkOmniConnectSettings settings;

    static const char* binaryExt = ".usd";
    static const char* asciiExt = ".usda";
    static const size_t bExtLen = strlen(binaryExt);
    static const size_t aExtLen = strlen(asciiExt);

    // Find out whether to output binary
    size_t fileNameLen = STRNLEN_PORTABLE(this->FileName, MAX_USER_STRLEN);
    if(fileNameLen >= aExtLen)
    {
      settings.OutputBinary = strcmp(&this->FileName[fileNameLen-bExtLen], binaryExt) == 0;
      if(!settings.OutputBinary && strcmp(&this->FileName[fileNameLen-aExtLen], asciiExt) != 0)
        return;
    }
    else
      return;

    const size_t extLen = settings.OutputBinary ? bExtLen : aExtLen;

    // Filter out the directory
    size_t dirEnd = fileNameLen;
    while(dirEnd > 0)
    {
      --dirEnd;
      if(this->FileName[dirEnd] == '\\' || this->FileName[dirEnd] == '/')
      {
        dirEnd++;
        break;
      }
    }

    if(dirEnd > 0)
    {
      settings.LocalOutputDirectory.assign(this->FileName, dirEnd);
      settings.RootLevelFileName.assign(this->FileName+dirEnd, this->FileName+fileNameLen-extLen);
      settings.OutputLocal = true;

      OmniConnectEnvironment omniEnv{ 0, 1 };

      omniConnectPass->Initialize(settings, omniEnv, 0.0);

      vtkOmniConnectRendererNode* rendererNode = omniConnectPass->GetSceneGraph();
      if(rendererNode)
      {
        rendererNode->SetRenderingEnabled(false); // Don't render, just output to USD

        vtkRenderState s(ren);
        omniConnectPass->Render(&s);
      }
    }
  }
}

// ----------------------------------------------------------------------------
void vtkOmniConnectUsdExporter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  if (this->FileName)
  {
    os << indent << "FileName: " << this->FileName << "\n";
  }
  else
  {
    os << indent << "FileName: (null)\n";
  }
}