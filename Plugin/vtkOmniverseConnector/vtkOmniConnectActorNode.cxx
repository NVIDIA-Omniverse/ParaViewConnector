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

#include "vtkOmniConnectActorNode.h"
#include "vtkOmniConnectRendererNode.h"
#include "vtkObjectFactory.h"
#include "vtkOmniConnectViewNodeFactory.h"
#include "vtkActor.h"
#include "vtkOpenGLGlyph3DMapper.h"

//============================================================================
vtkStandardNewMacro(vtkOmniConnectActorNode);

//----------------------------------------------------------------------------
vtkOmniConnectActorNode::vtkOmniConnectActorNode()
{
}

//----------------------------------------------------------------------------
vtkOmniConnectActorNode::~vtkOmniConnectActorNode() 
{
}

//----------------------------------------------------------------------------
void vtkOmniConnectActorNode::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkOmniConnectActorNode::Build(bool prepass)
{
  // Make sure the renderable is valid
  vtkProp3D* prop3D = ActorNodeBase.GetVtkProp3D();
  if (prop3D == nullptr)
    return;
  vtkActor* actor = vtkActor::SafeDownCast(prop3D);
  if (!actor)
    return;

  if (!vtkOmniConnectViewNodeFactory::IsSupportedActorMapper(actor))
  {
    // Just pass on to sub (actor) nodes
    this->Superclass::Build(prepass);
  }
  else
  {
    // Perform actor connect initialization if necessary
    ActorNodeBase.ConnectorBuild(prepass, vtkOmniConnectRendererNode::GetRendererNode(this));

    // Only perform mapper build once connect data is initialized
    if (ActorNodeBase.ConnectorIsInitialized())
      this->Superclass::Build(prepass);
  }
}

//----------------------------------------------------------------------------
void vtkOmniConnectActorNode::Render(bool prepass)
{
  // Make sure the renderable is valid
  if (ActorNodeBase.GetVtkProp3D() == nullptr)
    return;

  ActorNodeBase.ConnectorRender(prepass);
}

//------------------------------------------------------------------------------
void vtkOmniConnectActorNode::SetRenderable(vtkObject* obj)
{ 
  ActorNodeBase.SetVtkProp3D(obj);
  this->Superclass::SetRenderable(obj);
}