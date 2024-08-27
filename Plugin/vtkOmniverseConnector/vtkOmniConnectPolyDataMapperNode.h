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

#ifndef vtkOmniConnectPolyDataMapperNode_h
#define vtkOmniConnectPolyDataMapperNode_h

#include "OmniConnectData.h"

#include "vtkOmniverseConnectorModule.h" // For export macro
#include "vtkPolyDataMapperNode.h"

class vtkOmniConnectRendererNode;
class vtkOmniConnectActorNodeBase;
class vtkPolyData;
class vtkPolyDataMapper;
class vtkProperty;
class vtkOmniConnectPolyDataMapperNodeInternals;
class vtkDataArray;
class vtkDataObject;
class vtkCompositeDataDisplayAttributes;
struct OmniConnectTimeStep;
struct vtkOmniConnectTimeStep;

class VTKOMNIVERSECONNECTOR_EXPORT vtkOmniConnectPolyDataMapperNode : public vtkPolyDataMapperNode
{
public:
  static vtkOmniConnectPolyDataMapperNode* New();
  vtkTypeMacro(vtkOmniConnectPolyDataMapperNode, vtkPolyDataMapperNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * Perform the OmniConnect translation
   */
  void Build(bool prepass) override;
  void Render(bool prepass) override;

protected:
  vtkOmniConnectPolyDataMapperNode();
  ~vtkOmniConnectPolyDataMapperNode() override;

  virtual int GetRepresentation(vtkActor* actor);
  virtual bool HasNewShapeGeom() const { return false; }
  virtual bool PointsAreSpheres() const { return true; }
  virtual void GatherCustomPointAttributes(vtkPolyData* polyData, OmniConnectInstancerData& omniInstancerData) {}

  void ResetMaterialIds();
  void ExtractMapperProperties(vtkMapper* mapper);

  bool GetNodeNeedsProcessing(vtkOmniConnectRendererNode* rNode, vtkOmniConnectActorNodeBase* aNode,
    vtkOmniConnectTimeStep* omniTimeStep, vtkDataObject* poly, vtkCompositeDataDisplayAttributes* cda = nullptr) const;

  void RenderPoly(vtkOmniConnectRendererNode* rNode, vtkOmniConnectActorNodeBase* aNode, vtkActor* actor, vtkMapper* mapper, vtkPolyData* polyData, vtkProperty* prop,
    size_t dataObjectId, size_t materialId,
    double* overrideAmbientColor, double* overrideDiffuseColor, double* overrideSpecularColor, double overrideOpacity);
  
  vtkOmniConnectPolyDataMapperNodeInternals* Internals = nullptr;
  bool HasTranslucentGeometry = false;

private:
  vtkOmniConnectPolyDataMapperNode(const vtkOmniConnectPolyDataMapperNode&) = delete;
  void operator=(const vtkOmniConnectPolyDataMapperNode&) = delete;
};

#endif
