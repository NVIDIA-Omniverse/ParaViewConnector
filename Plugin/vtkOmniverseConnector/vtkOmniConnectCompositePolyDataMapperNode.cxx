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

#include "vtkOmniConnectCompositePolyDataMapperNode.h"

#include "vtkActor.h"
#include "vtkCompositeDataDisplayAttributes.h"
#include "vtkCompositePolyDataMapper.h"
#include "vtkMultiBlockDataSet.h"
#include "vtkMultiPieceDataSet.h"
#include "vtkOmniConnectActorNode.h"
#include "vtkOmniConnectRendererNode.h"
#include "vtkOmniConnectTimeStep.h"
#include "vtkOmniConnectMapperNodeCommon.h"
#include "vtkPolyData.h"
#include "vtkProperty.h"
#include "vtkRenderer.h"
#include "vtkSmartPointer.h"
#include "vtkInformation.h"

//============================================================================
vtkStandardNewMacro(vtkOmniConnectCompositePolyDataMapperNode);

//----------------------------------------------------------------------------
vtkOmniConnectCompositePolyDataMapperNode::vtkOmniConnectCompositePolyDataMapperNode() {}

//----------------------------------------------------------------------------
vtkOmniConnectCompositePolyDataMapperNode::~vtkOmniConnectCompositePolyDataMapperNode() {}

//----------------------------------------------------------------------------
void vtkOmniConnectCompositePolyDataMapperNode::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
vtkCompositeDataDisplayAttributes* vtkOmniConnectCompositePolyDataMapperNode::GetCompositeDisplayAttributes()
{
  vtkCompositePolyDataMapper* cpdm = vtkCompositePolyDataMapper::SafeDownCast(this->GetRenderable());
  return cpdm ? cpdm->GetCompositeDataDisplayAttributes() : nullptr;
}

//----------------------------------------------------------------------------
void vtkOmniConnectCompositePolyDataMapperNode::Render(bool prepass)
{
  if (prepass)
  {
    vtkOmniConnectRendererNode* rNode = vtkOmniConnectRendererNode::GetRendererNode(this);

    // we use a lot of params from the actor node itself
    vtkOmniConnectActorNodeBase* aNode = &(vtkOmniConnectActorNode::SafeDownCast(this->Parent)->ActorNodeBase);
    vtkActor* act = nullptr;

    // Guard against uninitialized actors or initialized ones that are invisible.
    if (!(act = OmniConnectActorExistsAndInitialized<vtkActor>(aNode)))
      return;

    double animTimeStep = act->GetPropertyKeys() ? act->GetPropertyKeys()->Get(vtkDataObject::DATA_TIME_STEP()) : 0.0f;
    vtkOmniConnectTimeStep* omniTimeStep = aNode->GetTimeStep(animTimeStep);

    // Prepare vtk dataentry cache for node rebuild (set to remove unless explicitly defined otherwise).
    omniTimeStep->ResetDataEntryCache();
    this->ResetMaterialIds();

    // render using the composite data attributes
    unsigned int flat_index = 0;
    unsigned int material_index = 0;
    vtkMapper* baseMapper = vtkMapper::SafeDownCast(this->GetRenderable());
    vtkDataObject* dobj = baseMapper ? baseMapper->GetInputDataObject(0, 0) : nullptr;

    if (dobj)
    {
      vtkCompositeDataDisplayAttributes* cda = this->GetCompositeDisplayAttributes();
      if (this->GetNodeNeedsProcessing(rNode, aNode, omniTimeStep, dobj, cda))
      {
        aNode->SetInputDataChanged();

        if (aNode->GetMaterialsChanged())
          this->ExtractMapperProperties(baseMapper); // perform potentially expensive mapper computations only once after material change

        vtkProperty* prop = act->GetProperty();

        vtkCompositeDataSet* compDs = vtkCompositeDataSet::SafeDownCast(dobj);
        if(compDs)
        {
          // Push base-values on the state stack.
          this->BlockState.Visibility.push(true);
          this->BlockState.Opacity.push(prop->GetOpacity());
          this->BlockState.AmbientColor.push(vtkColor3d(prop->GetAmbientColor()));
          this->BlockState.DiffuseColor.push(vtkColor3d(prop->GetDiffuseColor()));
          this->BlockState.SpecularColor.push(vtkColor3d(prop->GetSpecularColor()));

          // Recurse into block for individual polyData render calls
          this->RenderBlock(rNode, aNode, baseMapper, act, dobj, cda, flat_index, material_index);

          this->BlockState.Visibility.pop();
          this->BlockState.Opacity.pop();
          this->BlockState.AmbientColor.pop();
          this->BlockState.DiffuseColor.pop();
          this->BlockState.SpecularColor.pop(); 
        }
        else if(vtkPolyData* poly = vtkPolyData::SafeDownCast(dobj))
        {
          aNode->SetSubGeomProgress(0.0);

          baseMapper->ClearColorArrays();
          vtkProperty* property = act->GetProperty();
          this->RenderPoly(rNode, aNode, act, baseMapper, poly, property, 0, 0,
            property->GetAmbientColor(), property->GetDiffuseColor(), property->GetSpecularColor(), property->GetOpacity());

          aNode->SetSubGeomProgress(0.99);
        }
      }
      else
      {
        // Keep all vtk dataentry caches (dataEntryId)
        omniTimeStep->KeepDataEntryCache();
        // Make sure no earlier usd geometries are thrown out either (geomId)
        aNode->KeepTransferredGeometries();
      }
    }

    // Clean up vtk dataentry cache 
    omniTimeStep->CleanupDataEntryCache();
  }
}

//-----------------------------------------------------------------------------
void vtkOmniConnectCompositePolyDataMapperNode::RenderBlock(vtkOmniConnectRendererNode* rNode, vtkOmniConnectActorNodeBase* aNode,
  vtkMapper* baseMapper, vtkActor* actor, vtkDataObject* dobj, vtkCompositeDataDisplayAttributes* cda, unsigned int& flat_index, unsigned int& material_index)
{
  vtkProperty* prop = actor->GetProperty();
  // bool draw_surface_with_edges =
  //   (prop->GetEdgeVisibility() && prop->GetRepresentation() == VTK_SURFACE);
  vtkColor3d ecolor(prop->GetEdgeColor());

  bool overrides_visibility = (cda && cda->HasBlockVisibility(dobj));
  if (overrides_visibility)
  {
    this->BlockState.Visibility.push(cda->GetBlockVisibility(dobj));
  }

  bool overrides_opacity = (cda && cda->HasBlockOpacity(dobj));
  if (overrides_opacity)
  {
    this->BlockState.Opacity.push(cda->GetBlockOpacity(dobj));
  }

  bool overrides_color = (cda && cda->HasBlockColor(dobj));
  if (overrides_color)
  {
    vtkColor3d color = cda->GetBlockColor(dobj);
    this->BlockState.AmbientColor.push(color);
    this->BlockState.DiffuseColor.push(color);
    this->BlockState.SpecularColor.push(color);
  }
  
  if (overrides_opacity || overrides_color)
  {
    ++material_index;
  }
  size_t dataEntryId = flat_index;
  size_t materialId = material_index;

  // Advance flat-index. After this point, flat_index no longer points to this
  // block.
  flat_index++;

  vtkMultiBlockDataSet* mbds = vtkMultiBlockDataSet::SafeDownCast(dobj);
  vtkMultiPieceDataSet* mpds = vtkMultiPieceDataSet::SafeDownCast(dobj);
  if (mbds || mpds)
  {
    unsigned int numChildren = mbds ? mbds->GetNumberOfBlocks() : mpds->GetNumberOfPieces();
    for (unsigned int cc = 0; cc < numChildren; cc++)
    {
      vtkDataObject* child = mbds ? mbds->GetBlock(cc) : mpds->GetPiece(cc);
      if (child == nullptr)
      {
        // speeds things up when dealing with nullptr blocks (which is common with
        // AMRs).
        flat_index++;
        continue;
      }
      this->RenderBlock(rNode, aNode, baseMapper, actor, child, cda, flat_index, material_index);
    }
  }
  else if (dobj)
  {
    // do we have a entry for this dataset?
    // make sure we have an entry for this dataset
    vtkPolyData* poly = vtkPolyData::SafeDownCast(dobj);
    if (poly)
    {
      aNode->SetSubGeomProgress(0.0); // Start progress for a new subgeom (regardless of polydata visibility; visibility is only relevant on a per-actor level)

      if (this->BlockState.Visibility.top() == true)
      {
        vtkColor3d& aColor = this->BlockState.AmbientColor.top();
        vtkColor3d& dColor = this->BlockState.DiffuseColor.top();
        vtkColor3d& sColor = this->BlockState.SpecularColor.top();
        
        baseMapper->ClearColorArrays(); // prevents reuse of stale color arrays (if filled)
        this->RenderPoly(rNode, aNode, actor, baseMapper, poly, prop, dataEntryId, materialId,
          aColor.GetData(), dColor.GetData(), sColor.GetData(), this->BlockState.Opacity.top());
      }

      aNode->SetSubGeomProgress(0.99);
    }
  }

  if (overrides_color)
  {
    this->BlockState.AmbientColor.pop();
    this->BlockState.DiffuseColor.pop();
    this->BlockState.SpecularColor.pop();
  }
  if (overrides_opacity)
  {
    this->BlockState.Opacity.pop();
  }
  if (overrides_visibility)
  {
    this->BlockState.Visibility.pop();
  }
}
