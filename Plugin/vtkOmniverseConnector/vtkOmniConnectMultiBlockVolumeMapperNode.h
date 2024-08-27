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

#ifndef vtkOmniConnectMultiBlockVolumeMapperNode_h
#define vtkOmniConnectMultiBlockVolumeMapperNode_h

#include "vtkOmniverseConnectorModule.h" // For export macro
#include "vtkVolumeMapperNode.h"

#include "OmniConnectData.h"

class VTKOMNIVERSECONNECTOR_EXPORT vtkOmniConnectMultiBlockVolumeMapperNode : public vtkVolumeMapperNode
{
public:
  static vtkOmniConnectMultiBlockVolumeMapperNode* New();
  vtkTypeMacro(vtkOmniConnectMultiBlockVolumeMapperNode, vtkVolumeMapperNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  virtual void Build(bool prepass) override;

protected:
  vtkOmniConnectMultiBlockVolumeMapperNode();
  ~vtkOmniConnectMultiBlockVolumeMapperNode() override;

private:
  vtkOmniConnectMultiBlockVolumeMapperNode(const vtkOmniConnectMultiBlockVolumeMapperNode&) = delete;
  void operator=(const vtkOmniConnectMultiBlockVolumeMapperNode&) = delete;
};
#endif
