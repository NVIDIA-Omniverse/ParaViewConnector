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

#ifndef vtkOmniConnectActorNodeBase_h
#define vtkOmniConnectActorNodeBase_h

#include "OmniConnectData.h"
#include "vtkWeakPointer.h"
#include "vtkProp3D.h"

class vtkObject;
class vtkImageData;
class vtkOmniConnectActorNodeBaseInternals;
class vtkOmniConnectRendererNode;
struct vtkOmniConnectTimeStep;
struct vtkOmniConnectTextureCache;

class vtkOmniConnectActorNodeBase
{
public:
  vtkOmniConnectActorNodeBase();
  ~vtkOmniConnectActorNodeBase();

  bool HasConnectorContent() const;
  bool GetActorChanged() const { return this->ActorChanged; } // Any direct actor changes are recorded here. Read by MapperNode from Actor.
  bool GetMaterialsChanged() const { return this->MaterialsChanged; } // Any per-actor material changes are recorded here. Read by MapperNode from Actor.
  bool GetRepresentationChanged() const { return this->RepresentationChanged; }
  void SetInputDataChanged() { this->InputDataChanged = true; } // Any input data changes are recorded here. Written by MapperNode to Actor.
  void KeepTransferredGeometries();
  bool AddTransferredGeometry(size_t geomId, OmniConnectGeomType geomType); // Returns whether added geometry is new
  vtkOmniConnectTextureCache* UpdateTextureCache(vtkImageData* texData, size_t materialId, bool isColorTextureMap);

  size_t GetActorId();
  vtkOmniConnectTimeStep* GetTimeStep(double animTimeStep);
  
  void ConnectorBuild(bool prepass, vtkOmniConnectRendererNode* rendererNode);
  void ConnectorRender(bool prepass);

  void SetSubGeomProgress(double subGeomPercentage);
  void SetInRendererDestructor() { this->InRendererDestructor = true; };

  bool ConnectorIsInitialized() const;

  void SetVtkProp3D(vtkObject* obj);
  vtkProp3D* GetVtkProp3D() const { return VtkProp3D.Get(); }

protected:

  void DeleteActor();

  vtkOmniConnectActorNodeBaseInternals* Internals = nullptr;
  bool InRendererDestructor = false;

  bool ActorChanged = false;
  bool MaterialsChanged = false;
  bool RepresentationChanged = false;
  bool InputDataChanged = false;

  bool VisibilityChanged = false;
  bool TransformChanged = false;
  bool TextureChanged = false;
  bool GeometryChanged = false;

  // RendererNode fallback, in case Parentnode has already been destroyed before this node
  vtkWeakPointer<vtkOmniConnectRendererNode> RendererNode;
  // Hack to avoid that GetRenderable() can return an already destructed renderable
  vtkWeakPointer<vtkProp3D> VtkProp3D;
};

#endif
