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

#include "vtkOmniConnectViewNodeFactory.h"

#include "vtkObjectFactory.h"
#include "vtkOmniConnectActorNode.h"
#include "vtkOmniConnectRendererNode.h"
#include "vtkOmniConnectCompositePolyDataMapperNode.h"
#include "vtkOmniConnectVolumeNode.h"
#include "vtkOmniConnectVolumeMapperNode.h"
#include "vtkOmniConnectMultiBlockVolumeMapperNode.h"
#include "vtkOmniConnectImageSliceMapperNode.h"
#include "vtkOmniConnectGlyph3DMapperNode.h"
#include "vtkActor.h"
#include "vtkOpenGLPolyDataMapper.h"
#include "vtkCompositePolyDataMapper.h"
#include "vtkCompositePolyDataMapper.h"
#include "vtkGlyph3DMapper.h"


#include "OmniConnectData.h"

vtkViewNode* ren_maker()
{
  vtkOmniConnectRendererNode* vn = vtkOmniConnectRendererNode::New();
  return vn;
}

vtkViewNode* act_maker()
{
  vtkOmniConnectActorNode* vn = vtkOmniConnectActorNode::New();
  return vn;
}

vtkViewNode* vol_maker()
{
  vtkOmniConnectVolumeNode* vn = vtkOmniConnectVolumeNode::New();
  return vn;
}

vtkViewNode* pd_maker()
{
  vtkOmniConnectPolyDataMapperNode* vn = vtkOmniConnectPolyDataMapperNode::New();
  return vn;
}

vtkViewNode* vm_maker()
{
  vtkOmniConnectVolumeMapperNode* vn = vtkOmniConnectVolumeMapperNode::New();
  return vn;
}

vtkViewNode* mbvm_maker()
{
  vtkOmniConnectMultiBlockVolumeMapperNode* vn = vtkOmniConnectMultiBlockVolumeMapperNode::New();
  return vn;
}

vtkViewNode* cpd_maker()
{
  vtkOmniConnectCompositePolyDataMapperNode* vn = vtkOmniConnectCompositePolyDataMapperNode::New();
  return vn;
}

vtkViewNode* ism_maker()
{
  vtkOmniConnectImageSliceMapperNode* ism = vtkOmniConnectImageSliceMapperNode::New();
  return ism;
}

vtkViewNode* gm_maker()
{
  vtkOmniConnectGlyph3DMapperNode* gm = vtkOmniConnectGlyph3DMapperNode::New();
  return gm;
}

//============================================================================
vtkStandardNewMacro(vtkOmniConnectViewNodeFactory);

//----------------------------------------------------------------------------
bool vtkOmniConnectViewNodeFactory::IsSupportedActorMapper(vtkActor* actor)
{
  // Mappers which are supported as children of actor nodes
  vtkMapper* mapper = actor->GetMapper();
  return vtkOpenGLPolyDataMapper::SafeDownCast(mapper)
    || vtkCompositePolyDataMapper::SafeDownCast(mapper)
    || vtkGlyph3DMapper::SafeDownCast(mapper);
}

//----------------------------------------------------------------------------
vtkOmniConnectViewNodeFactory::vtkOmniConnectViewNodeFactory()
{
  // see vtkRenderWindow::GetRenderLibrary
  this->RegisterOverride("vtkOpenGLRenderer", ren_maker);
  this->RegisterOverride("vtkOpenGLActor", act_maker);
  this->RegisterOverride("vtkPVLODActor", act_maker);
  this->RegisterOverride("vtkVolume", vol_maker);
  this->RegisterOverride("vtkPVLODVolume", vol_maker);
  this->RegisterOverride("vtkOpenGLPolyDataMapper", pd_maker);
  this->RegisterOverride("vtkCompositePolyDataMapper", cpd_maker);
  this->RegisterOverride("vtkSmartVolumeMapper", vm_maker);
  this->RegisterOverride("vtkMultiBlockVolumeMapper", vm_maker);
  this->RegisterOverride("vtkOSPRayVolumeMapper", vm_maker);
  this->RegisterOverride("vtkOpenGLGPUVolumeRayCastMapper", vm_maker);
  this->RegisterOverride("vtkPVImageSliceMapper", ism_maker);
  this->RegisterOverride("vtkGlyph3DMapper", gm_maker);
}

//----------------------------------------------------------------------------
vtkOmniConnectViewNodeFactory::~vtkOmniConnectViewNodeFactory() {}

//----------------------------------------------------------------------------
void vtkOmniConnectViewNodeFactory::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

