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

#ifndef vtkOmniConnectCompositePolyDataMapperNode_h
#define vtkOmniConnectCompositePolyDataMapperNode_h

#include "vtkOmniConnectPolyDataMapperNode.h"
#include "vtkColor.h" 
#include <stack>                          

class vtkDataObject;
class vtkCompositePolyDataMapper;
class vtkOmniConnectRendererNode;
class vtkCompositeDataDisplayAttributes;

class VTKOMNIVERSECONNECTOR_EXPORT vtkOmniConnectCompositePolyDataMapperNode
  : public vtkOmniConnectPolyDataMapperNode
{
public:
  static vtkOmniConnectCompositePolyDataMapperNode* New();
  vtkTypeMacro(vtkOmniConnectCompositePolyDataMapperNode, vtkOmniConnectPolyDataMapperNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * Perform the OmniConnect translation
   */
  virtual void Render(bool prepass) override;

protected:
  vtkOmniConnectCompositePolyDataMapperNode();
  ~vtkOmniConnectCompositePolyDataMapperNode();

  class RenderBlockState
  {
  public:
    std::stack<bool> Visibility;
    std::stack<double> Opacity;
    std::stack<vtkColor3d> AmbientColor;
    std::stack<vtkColor3d> DiffuseColor;
    std::stack<vtkColor3d> SpecularColor;
  };

  RenderBlockState BlockState;
  void RenderBlock(vtkOmniConnectRendererNode* orn, vtkOmniConnectActorNodeBase* aNode,
    vtkMapper* baseMapper, vtkActor* actor,
    vtkDataObject* dobj, vtkCompositeDataDisplayAttributes* cda, 
    unsigned int& flat_index, unsigned int& material_index);

  virtual vtkCompositeDataDisplayAttributes* GetCompositeDisplayAttributes();

private:
  vtkOmniConnectCompositePolyDataMapperNode(const vtkOmniConnectCompositePolyDataMapperNode&) = delete;
  void operator=(const vtkOmniConnectCompositePolyDataMapperNode&) = delete;
};

#endif
