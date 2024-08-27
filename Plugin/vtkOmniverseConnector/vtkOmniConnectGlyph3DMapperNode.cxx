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

#include "vtkOmniConnectGlyph3DMapperNode.h"

#include "vtkMapper.h"
#include "vtkActor.h"
#include "vtkObjectFactory.h"
#include "vtkProperty.h"
#include "vtkInformation.h"
#include "vtkInformationIntegerKey.h"
#include "vtkInformationDoubleVectorKey.h"
#include "vtkGlyph3DMapper.h"
#include "vtkQuaternion.h"
#include "vtkBitArray.h"
#include "vtkOmniConnectActorNode.h"
#include "vtkOmniConnectMapperNodeCommon.h"
#include "vtkOmniConnectRendererNode.h"
#include "vtkOmniConnectTimeStep.h"
#include "OmniConnectUtilsExternal.h"

//============================================================================
vtkInformationKeyMacro(vtkOmniConnectGlyph3DMapperNode, GLYPHSHAPE, Integer);
vtkInformationKeyMacro(vtkOmniConnectGlyph3DMapperNode, GLYPHDIMS, DoubleVector);

// vtkMapper itself is abstract, so override here
class vtkOmniConnectGlyph3DMapper : public vtkGlyph3DMapper
{
public:
  vtkTypeMacro(vtkOmniConnectGlyph3DMapper, vtkGlyph3DMapper);
  static vtkOmniConnectGlyph3DMapper* New();
  void Render(vtkRenderer*, vtkActor*) override {}

  vtkDataArray* GetOrientations(vtkDataSet* input) { return this->GetOrientationArray(input); }
  vtkDataArray* GetScales(vtkDataSet* input) { return this->GetScaleArray(input); }
  vtkDataArray* GetMasks(vtkDataSet* input) { return this->GetMaskArray(input); }
};

vtkStandardNewMacro(vtkOmniConnectGlyph3DMapper);

//============================================================================
vtkStandardNewMacro(vtkOmniConnectGlyph3DMapperNode);

//----------------------------------------------------------------------------
vtkOmniConnectGlyph3DMapperNode::vtkOmniConnectGlyph3DMapperNode()
{

}

//----------------------------------------------------------------------------
vtkOmniConnectGlyph3DMapperNode::~vtkOmniConnectGlyph3DMapperNode()
{

}

//----------------------------------------------------------------------------
void vtkOmniConnectGlyph3DMapperNode::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
int vtkOmniConnectGlyph3DMapperNode::GetRepresentation(vtkActor* actor)
{
  return VTK_POINTS; // From a VTK perspective, treat all primitives as points
}

//----------------------------------------------------------------------------
bool vtkOmniConnectGlyph3DMapperNode::HasNewShapeGeom() const
{
  return this->HasNewGlyphShape;
}

//----------------------------------------------------------------------------
void vtkOmniConnectGlyph3DMapperNode::GatherCustomPointAttributes(vtkPolyData* polyData, OmniConnectInstancerData& omniInstancerData)
{
  switch(this->CurrentGlyphShape)
  {
    case SHAPE_CONE:
      omniInstancerData.DefaultShape = OmniConnectInstancerData::SHAPE_CONE;
      break;
    case SHAPE_CYLINDER:
      omniInstancerData.DefaultShape = OmniConnectInstancerData::SHAPE_CYLINDER;
      break;
    case SHAPE_CUBE:
      omniInstancerData.DefaultShape = OmniConnectInstancerData::SHAPE_CUBE;
      break;
    case SHAPE_ARROW:
      omniInstancerData.DefaultShape = OmniConnectInstancerData::SHAPE_ARROW;
      break;
    default:
      omniInstancerData.DefaultShape = OmniConnectInstancerData::SHAPE_SPHERE;
      break;
  }

  if(!this->SourceActorName.empty())
    omniInstancerData.ShapeSourceNameSubstr = this->SourceActorName.c_str();

  omniInstancerData.ShapeDims[0] = this->CurrentGlyphDims[0];
  omniInstancerData.ShapeDims[1] = this->CurrentGlyphDims[1];
  omniInstancerData.ShapeDims[2] = this->CurrentGlyphDims[2];

  vtkGlyph3DMapper* glm = vtkGlyph3DMapper::SafeDownCast(this->GetRenderable());
  vtkOmniConnectGlyph3DMapper* glyphMapper = reinterpret_cast<vtkOmniConnectGlyph3DMapper*>(glm);

  vtkOmniConnectRendererNode* rNode = vtkOmniConnectRendererNode::GetRendererNode(this);
  vtkOmniConnectTempArrays& tempArrays = rNode->GetTempArrays();

  if(glyphMapper->GetOrient())
  {
    vtkDataArray* orientArray = glyphMapper->GetOrientations(polyData);
    if (orientArray != nullptr && orientArray->GetNumberOfTuples() == omniInstancerData.NumPoints)
    {
      tempArrays.OrientationsArray.resize(omniInstancerData.NumPoints*4);
      float* quatOut = tempArrays.OrientationsArray.data();

      int orientMode = glyphMapper->GetOrientationMode();
      if(orientMode == vtkGlyph3DMapper::DIRECTION && orientArray->GetNumberOfComponents() == 3)
      {
        // Fix for cylinders which PV doesn't apply: direction coincides with cylinder's length (Y) axis, whereas for other shapes it's X
        auto dirToQuaternionF =  (this->CurrentGlyphShape == SHAPE_CYLINDER) ? DirectionToQuaternionY : DirectionToQuaternionX;
        if(orientArray->GetDataType() == VTK_FLOAT)
        {
          float* dirIn = reinterpret_cast<float*>(orientArray->GetVoidPointer(0));
          for(uint64_t ptIdx = 0; ptIdx < omniInstancerData.NumPoints; ++ptIdx, dirIn+=3)
          {
            float dirLength = vtkMath::Norm(dirIn);
            dirToQuaternionF(dirIn, dirLength, quatOut+ptIdx*4);
          }
        }
        else
        {
          for(uint64_t ptIdx = 0; ptIdx < omniInstancerData.NumPoints; ++ptIdx)
          {
            double dirInD[3];
            orientArray->GetTuple(ptIdx, dirInD);

            float dirIn[3] = {(float)dirInD[0], (float)dirInD[1], (float)dirInD[2]};
            float dirLength = vtkMath::Norm(dirIn);
            dirToQuaternionF(dirIn, dirLength, quatOut+ptIdx*4);
          }
        }

        omniInstancerData.Orientations = quatOut;
      }
      else if(orientMode == vtkGlyph3DMapper::ROTATION && orientArray->GetNumberOfComponents() == 3)
      {
        if(orientArray->GetDataType() == VTK_FLOAT)
        {
          float* rotIn = reinterpret_cast<float*>(orientArray->GetVoidPointer(0));
          for(uint64_t ptIdx = 0; ptIdx < omniInstancerData.NumPoints; ++ptIdx, rotIn+=3)
          {
            RotationToQuaternion(rotIn, quatOut+ptIdx*4);
          }
        }
        else
        {
          for(uint64_t ptIdx = 0; ptIdx < omniInstancerData.NumPoints; ++ptIdx)
          {
            double rotInD[3];
            orientArray->GetTuple(ptIdx, rotInD);

            float rotIn[3] = {(float)rotInD[0], (float)rotInD[1], (float)rotInD[2]};
            RotationToQuaternion(rotIn, quatOut+ptIdx*4);
          }
        }

        omniInstancerData.Orientations = quatOut;
      }
      else if(orientMode == vtkGlyph3DMapper::QUATERNION && orientArray->GetNumberOfComponents() == 4)
      {
        if(orientArray->GetDataType() == VTK_FLOAT)
        {
          omniInstancerData.Orientations =  reinterpret_cast<float*>(orientArray->GetVoidPointer(0));
        }
        else
        {
          for(uint64_t ptIdx = 0; ptIdx < omniInstancerData.NumPoints; ++ptIdx)
          {
            vtkQuaterniond quat;
            orientArray->GetTuple(ptIdx, quat.GetData());

            vtkQuaternionf qRot(quat[0], quat[1], quat[2], quat[3]);

            qRot.Get(quatOut+ptIdx*4);
          }
          omniInstancerData.Orientations = quatOut;
        }
      }
      omniInstancerData.OrientationsType = OmniConnectType::FLOAT4;
    }
  }

  omniInstancerData.UniformScale = glyphMapper->GetScaling() ? glyphMapper->GetScaleFactor() : 1.0; // Glyph mapper does not adhere to point size, so reset uniformscale
  if(glyphMapper->GetScaling())
  {
    vtkDataArray* scaleArray = glyphMapper->GetScales(polyData);

    bool scaleTypeCorrect = glyphMapper->GetScaleMode() != vtkGlyph3DMapper::SCALE_BY_COMPONENTS || scaleArray->GetNumberOfComponents() == 3;

    if (scaleArray != nullptr && scaleArray->GetNumberOfTuples() == omniInstancerData.NumPoints && scaleTypeCorrect)
    {
      tempArrays.ScalesArray.resize(omniInstancerData.NumPoints*3);

      double* range = glyphMapper->GetRange();
      double den = range[1] - range[0];
      if (den == 0.0)
        den = 1.0;

      for(uint64_t ptIdx = 0; ptIdx < omniInstancerData.NumPoints; ++ptIdx)
      {
        double scalex = 1.0f; // Scale of the omniconnect glyph at PV size 1
        double scaley = 1.0f;
        double scalez = 1.0f;

        double* tuple = scaleArray->GetTuple(ptIdx);
        switch (glyphMapper->GetScaleMode())
        {
          case vtkGlyph3DMapper::SCALE_BY_MAGNITUDE:
            scalex = scaley = scalez *= vtkMath::Norm(tuple, scaleArray->GetNumberOfComponents());
            break;
          case vtkGlyph3DMapper::SCALE_BY_COMPONENTS:
            scalex *= tuple[0];
            scaley *= tuple[1];
            scalez *= tuple[2];
            break;
          case vtkGlyph3DMapper::NO_DATA_SCALING:
          default:
            break;
        }

        // Clamp data scale if enabled
        if (glyphMapper->GetClamping() && glyphMapper->GetScaleMode() != vtkGlyph3DMapper::NO_DATA_SCALING)
        {
          scalex = (scalex < range[0] ? range[0] : (scalex > range[1] ? range[1] : scalex));
          scalex = (scalex - range[0]) / den;
          scaley = (scaley < range[0] ? range[0] : (scaley > range[1] ? range[1] : scaley));
          scaley = (scaley - range[0]) / den;
          scalez = (scalez < range[0] ? range[0] : (scalez > range[1] ? range[1] : scalez));
          scalez = (scalez - range[0]) / den;
        }

        float scaleFac = glyphMapper->GetScaleFactor();
        scalex *= scaleFac;
        scaley *= scaleFac;
        scalez *= scaleFac;

        tempArrays.ScalesArray[ptIdx*3] = scalex;
        tempArrays.ScalesArray[ptIdx*3+1] = scaley;
        tempArrays.ScalesArray[ptIdx*3+2] = scalez;
      }

      omniInstancerData.Scales = tempArrays.ScalesArray.data();
      omniInstancerData.ScalesType = OmniConnectType::FLOAT3;
    }
  }

  if(glyphMapper->GetMasking())
  {
    vtkBitArray* maskArray = vtkArrayDownCast<vtkBitArray>(glyphMapper->GetMasks(polyData)); 
    if (maskArray && maskArray->GetNumberOfTuples() == omniInstancerData.NumPoints && maskArray->GetNumberOfComponents() == 1)
    {
      //tempArrays.InvisibleIndicesArray.resize(0); // InvisibleIndicesArray is already initialized by GatherPointData()

      int numInvis = 0;
      for(uint64_t ptIdx = 0; ptIdx < omniInstancerData.NumPoints; ++ptIdx)
      {
        if(maskArray->GetValue(ptIdx) == 0)
        {
          tempArrays.InvisibleIndicesArray.push_back((int64_t)ptIdx);
          ++numInvis;
        }
      }

      omniInstancerData.NumInvisibleIndices = tempArrays.InvisibleIndicesArray.size();
      if(omniInstancerData.NumInvisibleIndices)
        omniInstancerData.InvisibleIndices = tempArrays.InvisibleIndicesArray.data();
    }
  }
}

//----------------------------------------------------------------------------
vtkCompositeDataDisplayAttributes* vtkOmniConnectGlyph3DMapperNode::GetCompositeDisplayAttributes()
{
  vtkGlyph3DMapper* glm = vtkGlyph3DMapper::SafeDownCast(this->GetRenderable());
  return glm ? glm->GetBlockAttributes() : nullptr;
}

//----------------------------------------------------------------------------
void vtkOmniConnectGlyph3DMapperNode::Build(bool prepass)
{
}

//----------------------------------------------------------------------------
void vtkOmniConnectGlyph3DMapperNode::Render(bool prepass)
{
  // Get property
  vtkOmniConnectActorNodeBase* aNode = &(vtkOmniConnectActorNode::SafeDownCast(this->Parent)->ActorNodeBase);
  vtkActor* act = nullptr;
  if (!(act = OmniConnectActorExistsAndInitialized<vtkActor>(aNode)))
      return;

  // Set the glyph shape
  this->HasNewGlyphShape = false;

  vtkInformation* actInfo = act->GetPropertyKeys();
  if(actInfo->Has(vtkOmniConnectGlyph3DMapperNode::GLYPHSHAPE()))
  {
    GlyphShape newGlyphShape = (GlyphShape)(actInfo->Get(vtkOmniConnectGlyph3DMapperNode::GLYPHSHAPE()));
    if(this->CurrentGlyphShape != newGlyphShape)
    {
      this->CurrentGlyphShape = newGlyphShape;
      this->HasNewGlyphShape = true;
    }
  }
  if(actInfo->Has(vtkOmniConnectGlyph3DMapperNode::GLYPHDIMS()))
  {
    double dims[3] = {0.5, 0.5, 0.5};
    actInfo->Get(vtkOmniConnectGlyph3DMapperNode::GLYPHDIMS(), dims);

    if(memcmp(this->CurrentGlyphDims, dims, sizeof(dims)))
    {
      memcpy(this->CurrentGlyphDims, dims, sizeof(dims));
      this->HasNewGlyphShape = true;
    }
  }

  bool clearGlyphShape = false;
  if(this->CurrentGlyphShape == SHAPE_EXTERNAL
    && actInfo->Has(vtkOmniConnectRendererNode::ACTORINPUTNAME()))
  {
    const char* actorInputName = actInfo->Get(vtkOmniConnectRendererNode::ACTORINPUTNAME());
    if(STRNLEN_PORTABLE(actorInputName, MAX_USER_STRLEN) > 0)
    {
      if(!ocutils::UsdNamesEqual(actorInputName, this->SourceActorName.c_str()))
      {
        this->SourceActorName.assign(actorInputName);
        ocutils::FormatUsdName(const_cast<char*>(this->SourceActorName.data()));
        this->HasNewGlyphShape = true;
      }
    }
    else
      clearGlyphShape = true;
  }
  else
    clearGlyphShape = true;

  if(clearGlyphShape && !this->SourceActorName.empty())
  {
    this->SourceActorName.clear();
    this->HasNewGlyphShape = true;
  }

  this->Superclass::Render(prepass);
}