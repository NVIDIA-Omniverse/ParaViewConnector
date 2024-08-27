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

#include "vtkOmniConnectPass.h"

#include "vtkObjectFactory.h"
#include "vtkOmniConnectRendererNode.h"
#include "vtkOmniConnectImageSliceMapperNode.h"
#include "vtkOmniConnectViewNodeFactory.h"
#include "vtkDefaultPass.h"
#include "vtkRenderState.h"
#include "vtkRenderer.h"
#include "vtkOpenGLError.h"
#include "vtkOpenGLRenderWindow.h"
#include "vtkOpenGLState.h"

// ----------------------------------------------------------------------------
vtkStandardNewMacro(vtkOmniConnectPass);

// ----------------------------------------------------------------------------
vtkOmniConnectPass::vtkOmniConnectPass()
{
}

// ----------------------------------------------------------------------------
vtkOmniConnectPass::~vtkOmniConnectPass()
{
  this->SetSceneGraph(nullptr);
}

// ----------------------------------------------------------------------------
void vtkOmniConnectPass::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

// ----------------------------------------------------------------------------
vtkCxxSetObjectMacro(vtkOmniConnectPass, SceneGraph, vtkOmniConnectRendererNode);

// ----------------------------------------------------------------------------
void vtkOmniConnectPass::Initialize(const vtkOmniConnectSettings& settings, const OmniConnectEnvironment& environment, double sceneTime)
{
  this->Settings = settings;

  this->SceneGraph = vtkOmniConnectRendererNode::New();
  bool initSuccess = this->SceneGraph->Initialize(this->Settings, environment);
  if (initSuccess)
  {
    this->SceneGraph->SetMyFactory(this->Factory);
    this->SceneGraph->SetSceneTime(sceneTime);
  }
  else
  {
    this->SetSceneGraph(nullptr);
  }
}

// ----------------------------------------------------------------------------
void vtkOmniConnectPass::Render(const vtkRenderState* s)
{
  if (!this->SceneGraph)
    return;

  vtkRenderer* ren = s->GetRenderer();
  vtkOpenGLRenderWindow* win = vtkOpenGLRenderWindow::SafeDownCast(ren->GetRenderWindow());

  // Standard opengl rendering
  if(this->SceneGraph->GetRenderingEnabled())
  {
    vtkOpenGLClearErrorMacro();

    this->ClearLights(ren);
    this->UpdateLightGeometry(ren);
    this->UpdateLights(ren);

    this->UpdateGeometry(ren, s->GetFrameBuffer());

    vtkOpenGLCheckErrorMacro("failed after Render");
  }

  this->SceneGraph->SetRenderable(ren);
  this->SceneGraph->SetOpenGLWindow(win);
  this->SceneGraph->TraverseAllPasses(); // builds the appropriate node hierarchy using the Scenegraph's factory (vtkOmniConnectViewNodeFactory), then traverses the nodes for rendering
}

void vtkOmniConnectPass::SetVtkPVImageSliceMapperPolyDataOffset(size_t offset)
{
  vtkOmniConnectImageSliceMapperNode::VtkPVImageSliceMapperPolyDataOffset = offset;
}