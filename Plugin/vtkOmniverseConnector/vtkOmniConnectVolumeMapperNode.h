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

#ifndef vtkOmniConnectVolumeMapperNode_h
#define vtkOmniConnectVolumeMapperNode_h

#include "vtkOmniverseConnectorModule.h" // For export macro
#include "vtkVolumeMapperNode.h"

#include <vector>

class vtkOmniConnectActorNodeBase;
class vtkOmniConnectVolumeMapperNodeInternals;
class vtkVolume;
class vtkVolumeProperty;
class vtkImageData;

class VTKOMNIVERSECONNECTOR_EXPORT vtkOmniConnectVolumeMapperNode : public vtkVolumeMapperNode
{
public:
  static vtkOmniConnectVolumeMapperNode* New();
  vtkTypeMacro(vtkOmniConnectVolumeMapperNode, vtkVolumeMapperNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * Make OmniConnect calls to render me.
   */
  void Build(bool prepass) override;
  void Render(bool prepass) override;

protected:
  vtkOmniConnectVolumeMapperNode();
  ~vtkOmniConnectVolumeMapperNode() override;

  void ResetMaterialIds();
  void UpdateVolSpecificChanges(vtkVolumeMapper* mapper, vtkVolumeProperty* volProperty);
  void RenderVolume(vtkOmniConnectActorNodeBase* aNode, vtkVolume* actor, vtkVolumeMapper* mapper, vtkImageData* volData, size_t dataEntryId, size_t materialId);

  void UpdateTransferFunctions(vtkVolumeProperty* volProperty, double* volRange);

  static constexpr int NumTfValues = 128;
  std::vector<float> TfValues;
  std::vector<float> TfOValues;
  double TfRange[2] = { 0.0, 1.0 };

  vtkOmniConnectVolumeMapperNodeInternals* Internals = nullptr;

private:
  vtkOmniConnectVolumeMapperNode(const vtkOmniConnectVolumeMapperNode&) = delete;
  void operator=(const vtkOmniConnectVolumeMapperNode&) = delete;
};
#endif
