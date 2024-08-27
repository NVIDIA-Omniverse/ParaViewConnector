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

#include "vtkOmniConnectImageSliceMapperNode.h"

#include "vtkMapper.h"
#include "vtkActor.h"
#include "vtkObjectFactory.h"
#include "vtkProperty.h"
#include "vtkInformation.h"
#include "vtkOmniConnectActorNode.h"
#include "vtkOmniConnectRendererNode.h"

typedef vtkOmniConnectRendererNode OmniInformationBase;

//============================================================================
vtkStandardNewMacro(vtkOmniConnectImageSliceMapperNode);

//----------------------------------------------------------------------------
size_t vtkOmniConnectImageSliceMapperNode::VtkPVImageSliceMapperPolyDataOffset = 0;

//----------------------------------------------------------------------------
vtkOmniConnectImageSliceMapperNode::vtkOmniConnectImageSliceMapperNode()
{
}

//----------------------------------------------------------------------------
vtkOmniConnectImageSliceMapperNode::~vtkOmniConnectImageSliceMapperNode()
{
}

//----------------------------------------------------------------------------
void vtkOmniConnectImageSliceMapperNode::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
void vtkOmniConnectImageSliceMapperNode::Build(bool prepass)
{
  if(prepass)
  {
    vtkMapper* mapper = vtkMapper::SafeDownCast(this->GetRenderable());
    if(!mapper)
      return; 
      
    char* refToSliceActorPtr = ((char*)mapper) + VtkPVImageSliceMapperPolyDataOffset;
    vtkActor* slicePolyActor = *(reinterpret_cast<vtkActor**>(refToSliceActorPtr));

    if (slicePolyActor)
    {
      // Take over the name from the parent
      vtkOmniConnectActorNodeBase* aNode = &(vtkOmniConnectActorNode::SafeDownCast(this->Parent)->ActorNodeBase);
      vtkActor* parentActor = vtkActor::SafeDownCast(aNode->GetVtkProp3D());

      vtkInformation* parentInfo = parentActor->GetPropertyKeys();
      vtkInformation* sliceInfo = slicePolyActor->GetPropertyKeys();
      if (sliceInfo && !sliceInfo->Has(OmniInformationBase::ACTORNAME())
        && parentInfo && parentInfo->Has(OmniInformationBase::ACTORNAME()))
      {
        const char* parentName = parentInfo->Get(OmniInformationBase::ACTORNAME());
        sliceInfo->Set(OmniInformationBase::ACTORNAME(), parentName);
      }

      // Add slice poly actor as a normal actor with polydata to the hierarchy.
      this->PrepareNodes();
      this->AddMissingNode(slicePolyActor);
      this->RemoveUnusedNodes();
    }
  }

  this->Superclass::Build(prepass);
}

//----------------------------------------------------------------------------
void vtkOmniConnectImageSliceMapperNode::Render(bool prepass)
{
  this->Superclass::Render(prepass);
}