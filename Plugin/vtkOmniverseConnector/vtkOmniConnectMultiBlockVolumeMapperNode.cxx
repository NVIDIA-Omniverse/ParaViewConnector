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

#include "vtkOmniConnectMultiBlockVolumeMapperNode.h"

#include "vtkMultiBlockVolumeMapper.h"
#include "vtkObjectFactory.h"
#include "vtkCollection.h"

//============================================================================
vtkStandardNewMacro(vtkOmniConnectMultiBlockVolumeMapperNode);

//------------------------------------------------------------------------------
vtkOmniConnectMultiBlockVolumeMapperNode::vtkOmniConnectMultiBlockVolumeMapperNode()
{
}

//------------------------------------------------------------------------------
vtkOmniConnectMultiBlockVolumeMapperNode::~vtkOmniConnectMultiBlockVolumeMapperNode()
{
}

//------------------------------------------------------------------------------
void vtkOmniConnectMultiBlockVolumeMapperNode::PrintSelf(ostream& os, vtkIndent indent)
{
}

//------------------------------------------------------------------------------
void vtkOmniConnectMultiBlockVolumeMapperNode::Build(bool prepass)
{
  
  if (prepass)
  {
    
    //vtkMultiBlockVolumeMapper* mbMapper = vtkMultiBlockVolumeMapper::SafeDownCast(this->GetRenderable());
    //
    //vtkCollection* subMappers = vtkCollection::New();
    //for (vtkVolumeMapper* sm : mbMapper->GetMappers())
    //  subMappers->AddItem(sm);
    //
    //this->PrepareNodes();
    //this->AddMissingNodes(subMappers);
    //this->RemoveUnusedNodes();
    //
    //subMappers->Delete();
  }
}
