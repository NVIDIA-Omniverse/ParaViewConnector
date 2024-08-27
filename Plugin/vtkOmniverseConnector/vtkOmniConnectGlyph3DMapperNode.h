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

#ifndef vtkOmniConnectGlyph3DMapperNode_h
#define vtkOmniConnectGlyph3DMapperNode_h

#include "OmniConnectData.h"

#include "vtkOmniverseConnectorModule.h" // For export macro
#include "vtkOmniConnectCompositePolyDataMapperNode.h"

#include <string>

class vtkMapper;
class vtkInformationIntegerKey;
class vtkInformationDoubleVectorKey;

class VTKOMNIVERSECONNECTOR_EXPORT vtkOmniConnectGlyph3DMapperNode : public vtkOmniConnectCompositePolyDataMapperNode
{
public:
  static vtkOmniConnectGlyph3DMapperNode* New();
  vtkTypeMacro(vtkOmniConnectGlyph3DMapperNode, vtkOmniConnectCompositePolyDataMapperNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * Perform the OmniConnect translation
   */
  virtual void Build(bool prepass) override;
  virtual void Render(bool prepass) override;

  // The glyph shape information key (encoded into parent actor)
  enum GlyphShape
  {
    SHAPE_SPHERE,
    SHAPE_CYLINDER,
    SHAPE_CONE,
    SHAPE_CUBE,
    SHAPE_ARROW,
    SHAPE_EXTERNAL
  };

  static vtkInformationIntegerKey* GLYPHSHAPE();
  static vtkInformationDoubleVectorKey* GLYPHDIMS();

protected:
  vtkOmniConnectGlyph3DMapperNode();
  ~vtkOmniConnectGlyph3DMapperNode() override;

  int GetRepresentation(vtkActor* actor) override;
  bool HasNewShapeGeom() const override;
  bool PointsAreSpheres() const override { return CurrentGlyphShape == SHAPE_SPHERE; }
  void GatherCustomPointAttributes(vtkPolyData* polyData, OmniConnectInstancerData& omniInstancerData) override;
  vtkCompositeDataDisplayAttributes* GetCompositeDisplayAttributes() override;

  GlyphShape CurrentGlyphShape = SHAPE_SPHERE;
  double CurrentGlyphDims[3] = {0.5, 0.5, 0.5};
  bool HasNewGlyphShape = false;

  std::string SourceActorName;

private:
  vtkOmniConnectGlyph3DMapperNode(const vtkOmniConnectGlyph3DMapperNode&) = delete;
  void operator=(const vtkOmniConnectGlyph3DMapperNode&) = delete;
};

#endif
