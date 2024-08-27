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

#include "vtkOmniConnectPolyDataMapperNode.h"

#include "vtkOmniConnectVtkToOmni.h"
#include "vtkOmniConnectMapperNodeCommon.h"
#include "vtkOmniConnectActorNode.h"
#include "vtkOmniConnectRendererNode.h"
#include "vtkOmniConnectActorCache.h"
#include "vtkOmniConnectTextureCache.h"
#include "vtkOmniConnectTimeStep.h"
#include "vtkOmniConnectImageWriter.h"
#include "vtkOmniConnectLogCallback.h"
#include "vtkActor.h"
#include "vtkDataArray.h"
#include "vtkFloatArray.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkDataObject.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkCellData.h"
#include "vtkPolyData.h"
#include "vtkMapper.h"
#include "vtkPolygon.h"
#include "vtkProperty.h"
#include "vtkRenderer.h"
#include "vtkTexture.h"
#include "vtkUnsignedCharArray.h"
#include "vtkPolyDataNormals.h"
#include "vtkPiecewiseFunction.h"
#include "vtkScalarsToColors.h"

#if VTK_MODULE_ENABLE_VTK_RenderingRayTracing
#include "vtkOSPRayActorNode.h"
#endif

#include <map>
#include <cstring>

//============================================================================
namespace
{
  template<typename T>
  class EmptyVector
  {
  public:
    inline size_t size() const { return 0; };
    inline void resize(size_t) {};
    inline void push_back(const T&) {}
  };

  void MapScalesThroughPWF(vtkDataArray* scaleArray,  vtkPiecewiseFunction* scaleFunction, vtkOmniConnectTempArrays& tempArrays)
  {
    tempArrays.ScalesArray.resize(scaleArray->GetNumberOfTuples());

    for (vtkIdType i = 0; i < scaleArray->GetNumberOfTuples(); ++i)
    {
      tempArrays.ScalesArray[i] = static_cast<float>(scaleFunction->GetValue(*scaleArray->GetTuple(i)));
    }
  }

  void SetUsedVertexBuffer(vtkCellArray *cells,
    std::vector<int> &vertexToCell)
  {
    const vtkIdType* indices = nullptr;
    vtkIdType npts(0);
    if (!cells->GetNumberOfCells())
    {
      return;
    }

    int cell_id = 0;
    for (cells->InitTraversal(); cells->GetNextCell(npts, indices); )
    {
      for (int i = 0; i < npts; ++i)
      {
        unsigned int vi = (static_cast<unsigned int>(*(indices++)));
        vertexToCell[vi] = cell_id;
      }
      cell_id++;
    }
  }

  //----------------------------------------------------------------------------
  //Description:
  //Homogenizes lines into a flat list of line segments, each containing two point indexes
  //At same time creates a reverse cell index array for obtaining cell quantities for points
  template<typename T>
  void CreateLineIndexBuffer(vtkCellArray *cells,
    std::vector<unsigned int>& indexArray,
    T& reverseArray)
  {
    //TODO: restore the preallocate and append to offset features I omitted
    const vtkIdType* indices = nullptr;
    vtkIdType npts(0);
    if (!cells->GetNumberOfCells())
    {
      return;
    }

    unsigned int cell_id = 0;
    for (cells->InitTraversal(); cells->GetNextCell(npts, indices); )
    {
      for (int i = 0; i < npts - 1; ++i)
      {
        indexArray.push_back(static_cast<unsigned int>(indices[i]));
        indexArray.push_back(static_cast<unsigned int>(indices[i + 1]));
        reverseArray.push_back(cell_id);
        reverseArray.push_back(cell_id);
      }
      cell_id++;
    }
  }

  //----------------------------------------------------------------------------
  //Description:
  //Homogenizes polygons into a flat list of line segments, each containing two point indexes.
  //At same time creates a reverse cell index array for obtaining cell quantities for points
  //This differs from CreateLineIndexBuffer in that it closes loops, making a segment from last point
  //back to first.
  template<typename T>
  void CreateTriangleLineIndexBuffer(vtkCellArray *cells,
    std::vector<unsigned int>& indexArray,
    T& reverseArray)
  {
    //TODO: restore the preallocate and append to offset features I omitted
    const vtkIdType* indices = nullptr;
    vtkIdType npts(0);
    if (!cells->GetNumberOfCells())
    {
      return;
    }

    unsigned int cell_id = 0;
    for (cells->InitTraversal(); cells->GetNextCell(npts, indices); )
    {
      for (int i = 0; i < npts; ++i)
      {
        indexArray.push_back(static_cast<unsigned int>(indices[i]));
        indexArray.push_back(static_cast<unsigned int>
          (indices[i < npts - 1 ? i + 1 : 0]));
        reverseArray.push_back(cell_id);
        reverseArray.push_back(cell_id);
      }
      cell_id++;
    }
  }

  template<typename T>
  void CreateTriangleIndexBuffer(vtkCellArray* cells, vtkPoints* points,
    std::vector<unsigned int>& indexArray,
    T& reverseArray)
  {
    //TODO: restore the preallocate and append to offset features I omitted
    const vtkIdType* indices = nullptr;
    vtkIdType npts(0);
    if (!cells->GetNumberOfCells())
    {
      return;
    }
    unsigned int cell_id = 0;
    // the following are only used if we have to triangulate a polygon
    // otherwise they just sit at NULL
    vtkPolygon *polygon = NULL;
    vtkIdList *tris = NULL;
    vtkPoints *triPoints = NULL;

    for (cells->InitTraversal(); cells->GetNextCell(npts, indices); )
    {
      // ignore degenerate triangles
      if (npts < 3)
      {
        cell_id++;
        continue;
      }

      //Instead of this whole construct, could just always make a triangle fan such as the simple cases, or as in AppendTrianglesWorker in vtkOpenGLIndexBufferObject.cxx

      // triangulate needed
      if (npts > 3)
      {
        // special case for quads, penta, hex which are common
        if (npts == 4)
        {
          indexArray.push_back(static_cast<unsigned int>(indices[0]));
          indexArray.push_back(static_cast<unsigned int>(indices[1]));
          indexArray.push_back(static_cast<unsigned int>(indices[2]));
          indexArray.push_back(static_cast<unsigned int>(indices[0]));
          indexArray.push_back(static_cast<unsigned int>(indices[2]));
          indexArray.push_back(static_cast<unsigned int>(indices[3]));
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
        }
        else if (npts == 5)
        {
          indexArray.push_back(static_cast<unsigned int>(indices[0]));
          indexArray.push_back(static_cast<unsigned int>(indices[1]));
          indexArray.push_back(static_cast<unsigned int>(indices[2]));
          indexArray.push_back(static_cast<unsigned int>(indices[0]));
          indexArray.push_back(static_cast<unsigned int>(indices[2]));
          indexArray.push_back(static_cast<unsigned int>(indices[3]));
          indexArray.push_back(static_cast<unsigned int>(indices[0]));
          indexArray.push_back(static_cast<unsigned int>(indices[3]));
          indexArray.push_back(static_cast<unsigned int>(indices[4]));
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
        }
        else if (npts == 6)
        {
          indexArray.push_back(static_cast<unsigned int>(indices[0]));
          indexArray.push_back(static_cast<unsigned int>(indices[1]));
          indexArray.push_back(static_cast<unsigned int>(indices[2]));
          indexArray.push_back(static_cast<unsigned int>(indices[0]));
          indexArray.push_back(static_cast<unsigned int>(indices[2]));
          indexArray.push_back(static_cast<unsigned int>(indices[3]));
          indexArray.push_back(static_cast<unsigned int>(indices[0]));
          indexArray.push_back(static_cast<unsigned int>(indices[3]));
          indexArray.push_back(static_cast<unsigned int>(indices[5]));
          indexArray.push_back(static_cast<unsigned int>(indices[3]));
          indexArray.push_back(static_cast<unsigned int>(indices[4]));
          indexArray.push_back(static_cast<unsigned int>(indices[5]));
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
        }
        else // 7 sided polygon or higher, do a full smart triangulation
        {
          if (!polygon)
          {
            polygon = vtkPolygon::New();
            tris = vtkIdList::New();
            triPoints = vtkPoints::New();
          }

          vtkIdType *triIndices = new vtkIdType[npts];
          triPoints->SetNumberOfPoints(npts);
          for (int i = 0; i < npts; ++i)
          {
            int idx = indices[i];
            triPoints->SetPoint(i, points->GetPoint(idx));
            triIndices[i] = i;
          }
          polygon->Initialize(npts, triIndices, triPoints);
          polygon->TriangulateLocalIds(0, tris);
          for (int j = 0; j < tris->GetNumberOfIds(); ++j)
          {
            indexArray.push_back(static_cast<unsigned int>
              (indices[tris->GetId(j)]));
            reverseArray.push_back(cell_id);
          }
          delete[] triIndices;
        }
      }
      else
      {
        indexArray.push_back(static_cast<unsigned int>(*(indices++)));
        indexArray.push_back(static_cast<unsigned int>(*(indices++)));
        indexArray.push_back(static_cast<unsigned int>(*(indices++)));
        reverseArray.push_back(cell_id);
        reverseArray.push_back(cell_id);
        reverseArray.push_back(cell_id);
      }

      cell_id++;
    }
    if (polygon)
    {
      polygon->Delete();
      tris->Delete();
      triPoints->Delete();
    }
  }

  template<typename T>
  void CreateStripIndexBuffer(vtkCellArray *cells,
    std::vector<unsigned int>& indexArray,
    T& reverseArray,
    bool wireframeTriStrips)
  {
    if (!cells->GetNumberOfCells())
    {
      return;
    }
    unsigned int cell_id = 0;

    const vtkIdType *pts = nullptr;
    vtkIdType npts = 0;

    size_t triCount = cells->GetNumberOfConnectivityEntries()
      - 3 * cells->GetNumberOfCells();
    size_t targetSize = wireframeTriStrips ? 2 * (triCount * 2 + 1)
      : triCount * 3;
    indexArray.reserve(indexArray.size() + targetSize);

    if (wireframeTriStrips)
    {
      for (cells->InitTraversal(); cells->GetNextCell(npts, pts); )
      {
        indexArray.push_back(static_cast<unsigned int>(pts[0]));
        indexArray.push_back(static_cast<unsigned int>(pts[1]));
        reverseArray.push_back(cell_id);
        reverseArray.push_back(cell_id);
        for (int j = 0; j < npts - 2; ++j)
        {
          indexArray.push_back(static_cast<unsigned int>(pts[j]));
          indexArray.push_back(static_cast<unsigned int>(pts[j + 2]));
          indexArray.push_back(static_cast<unsigned int>(pts[j + 1]));
          indexArray.push_back(static_cast<unsigned int>(pts[j + 2]));
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
        }
        cell_id++;
      }
    }
    else
    {
      for (cells->InitTraversal(); cells->GetNextCell(npts, pts); )
      {
        for (int j = 0; j < npts - 2; ++j)
        {
          indexArray.push_back(static_cast<unsigned int>(pts[j]));
          indexArray.push_back(static_cast<unsigned int>(pts[j + 1 + j % 2]));
          indexArray.push_back(static_cast<unsigned int>(pts[j + 1 + (j + 1) % 2]));
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
          reverseArray.push_back(cell_id);
        }
        cell_id++;
      }
    }
  }

  template<typename T>
  void MakeConnectivity(vtkPolyData *poly,
    int representation,
    size_t numVerts,
    std::vector<unsigned int>& indexArray,
    T& indexToCell,
    std::vector<int>* vertexToCell,
    int filterPrim, //0 for points, 1 for lines, 2 for triangles
    bool stickLinesEnabled = false,
    bool stickWireframeEnabled = false,
    bool isSticks = false // connectivity is created for stick geometry, or for curve geometry
  )
  {
    indexArray.resize(0);
    //faceVertCounts.resize(0);
    indexToCell.resize(0);
    if (vertexToCell != nullptr)
    {
      vertexToCell->resize(numVerts);
      memset(&(*vertexToCell)[0], -1, sizeof(int)*numVerts);
    }

    vtkCellArray *prims[4];
    prims[0] = poly->GetVerts();
    prims[1] = poly->GetLines();
    prims[2] = poly->GetPolys();
    prims[3] = poly->GetStrips();

    switch (representation)
    {
    case VTK_POINTS:
    {
      if (filterPrim == 0)
      {
        SetUsedVertexBuffer(prims[0], *vertexToCell);
        SetUsedVertexBuffer(prims[1], *vertexToCell);
        SetUsedVertexBuffer(prims[2], *vertexToCell);
        SetUsedVertexBuffer(prims[3], *vertexToCell);
      }
      break;
    }
    case VTK_WIREFRAME:
    {
      if (filterPrim == 0)
        SetUsedVertexBuffer(prims[0], *vertexToCell);
      if (filterPrim == 1)
      {
        if(isSticks == stickLinesEnabled) // Get line data if building sticks with sticks enabled (for lines) or building curves with curves enabled
          CreateLineIndexBuffer(prims[1], indexArray, indexToCell);
        if (isSticks == stickWireframeEnabled) // Get wireframe data if building sticks with sticks enabled (for wireframe) or building curves with curves enabled
          CreateTriangleLineIndexBuffer(prims[2], indexArray, indexToCell);
          CreateStripIndexBuffer(prims[3], indexArray, indexToCell, true);
      }
      break;
    }
    default:
    {
      if (filterPrim == 0)
        SetUsedVertexBuffer(prims[0], *vertexToCell);
      if (filterPrim == 1)
      {
        if (isSticks == stickLinesEnabled)
          CreateLineIndexBuffer(prims[1], indexArray, indexToCell);
      }
      if (filterPrim == 2)
      {
        CreateTriangleIndexBuffer(prims[2], poly->GetPoints(), indexArray, indexToCell);
        CreateStripIndexBuffer(prims[3], indexArray, indexToCell, false);
      }
      break;
    }
    }
  }

  vtkUnsignedCharArray* GetColorsAndInterpolation(vtkMapper* mapper, vtkPolyData* polyData, bool& isCellColors)
  {
    int cellFlag = 0;
    isCellColors = false; // Default is per-vertex color array (cellFlag 0)

    // Get the color array. Not clear whether it is per-vertex or per-cell.
    vtkUnsignedCharArray* colors = mapper->GetColorMapColors();
    // Determine the cellflag, which will indicate the nature of the colors array.
    mapper->GetAbstractScalars(polyData, mapper->GetScalarMode(),
      mapper->GetArrayAccessMode(), mapper->GetArrayId(), mapper->GetArrayName(), cellFlag);

    if (cellFlag == 2)
      colors = nullptr; // Unset color array in case of constant colors
    else if (cellFlag == 1)
      isCellColors = true; // Array contains per-prim colors

    return colors;
  }

  bool DetermineVertexColors(vtkMapper* mapper, vtkPolyData* polyData)
  {
    bool IsCellColors = 0;
    vtkUnsignedCharArray* colors = GetColorsAndInterpolation(mapper, polyData, IsCellColors);

    return colors != 0;
  }

  vtkDataArray* GetTextureCoordinates(vtkMapper* mapper, vtkPolyData* polyData, bool hasTexture)
  {
    vtkDataArray* tc = polyData->GetPointData() ? polyData->GetPointData()->GetTCoords() : nullptr;
    if (hasTexture)
    {
      vtkFloatArray* colorCoords = mapper->GetColorCoordinates();
      if (mapper->GetInterpolateScalarsBeforeMapping() && colorCoords)
      {
        assert(colorCoords->GetNumberOfTuples() == polyData->GetPoints()->GetNumberOfPoints());

        tc = colorCoords;
      }

      if (tc && tc->GetNumberOfComponents() != 2)
      {
        vtkGenericWarningMacro("Only 2-component texcoords supported");
        tc = nullptr;
      }
    }
    else
    {
      tc = nullptr; // !hasTextures => no tex coords (vertex coloring requires !hasTextures, so can assume no texcoords)
    }

    return tc;
  }

  void GatherMaterialData(vtkProperty* prop, int texId, OmniConnectMaterialData& matData,
    double* overrideAmbientColor, double* overrideDiffuseColor, double* overrideSpecularColor, double overrideOpacity, bool useVertexColors, bool hasTranslucentGeometry, bool opacityMappingEnabled
#if defined(USE_MDL_MATERIALS) && USE_CUSTOM_POINT_SHADER
    , bool hasPoints
#endif 
    )
  {
    matData.TexId = texId;

    double luminosity = 0.0;
#if VTK_MODULE_ENABLE_VTK_RenderingRayTracing
    luminosity = vtkOSPRayActorNode::GetLuminosity(prop);
#endif

    matData.UseVertexColors = useVertexColors;
    matData.HasTranslucency = hasTranslucentGeometry || overrideOpacity < 1.0;
    matData.Opacity = (float)overrideOpacity;
    matData.OpacityMapped = opacityMappingEnabled;
    matData.Diffuse[0] = (float)overrideDiffuseColor[0];
    matData.Diffuse[1] = (float)overrideDiffuseColor[1];
    matData.Diffuse[2] = (float)overrideDiffuseColor[2];
    matData.Specular[0] = (float)overrideDiffuseColor[0];
    matData.Specular[1] = (float)overrideDiffuseColor[1];
    matData.Specular[2] = (float)overrideDiffuseColor[2];

    matData.EmissiveIntensity = (float)((luminosity * 100.0));

    if (prop->GetInterpolation() == VTK_PBR)
    {
      matData.EmissiveTextureData = prop->GetTexture("emissiveTex");

      matData.Emissive[0] = (float)prop->GetEmissiveFactor()[0];
      matData.Emissive[1] = (float)prop->GetEmissiveFactor()[1];
      matData.Emissive[2] = (float)prop->GetEmissiveFactor()[2];
      matData.Roughness = (float)prop->GetRoughness();
      matData.Metallic = (float)prop->GetMetallic();
      matData.Ior = (float)prop->GetBaseIOR();
    }

#if defined(USE_MDL_MATERIALS) && USE_CUSTOM_POINT_SHADER
    matData.UsePointShader = hasPoints;
#endif 
  }

  vtkFloatArray* GetPolyDataNormals(vtkPolyData* polyData, bool useCellNormals)
  {
    return useCellNormals ?
      vtkArrayDownCast<vtkFloatArray>(polyData->GetCellData()->GetNormals()) :
      vtkArrayDownCast<vtkFloatArray>(polyData->GetPointData()->GetNormals());
  }

  void FixUpdatedGenericArrays(OmniConnectGenericArray* updatedGenericArrays, size_t numUga, vtkFieldData* fieldData, bool cellData)
  {
    if(fieldData)
    {
      const char* namePrefix = cellData ? vtkOmniConnectGenericCellArrayPrefix : vtkOmniConnectGenericPointArrayPrefix;

      for(int i = 0; i < numUga; ++i)
      {
        OmniConnectGenericArray& genArray = updatedGenericArrays[i];
        if (genArray.PerPoly == cellData)
        {
          vtkDataArray* vtkArray = fieldData->GetArray(genArray.Name + strlen(namePrefix));
          if (vtkArray)
          {
            genArray.Data = vtkArray->GetVoidPointer(0);
            genArray.NumElements = vtkArray->GetNumberOfTuples();
          }
        }
      }
    }
  }

  void GatherConnectedGeom(size_t& numVerts, vtkDataArray*& points, vtkFloatArray*& normals, vtkUnsignedCharArray*& colors, vtkDataArray*& texcoords,
    vtkDataArray*& scaleArray, vtkPiecewiseFunction*& scaleFunction,
    bool& useCellNormals, bool& useCellColors, bool& forceConsistentWinding,
    vtkOmniConnectTempArrays& tempArrays, vtkMapper* mapper, vtkProperty* prop, vtkPolyData* polyData, int representation, vtkPolyDataNormals* normalGenerator,
    bool hasTexture, int geomType, bool stickLinesEnabled = false, bool stickWireframeEnabled = false, bool isSticks = false,
    OmniConnectGenericArray* updatedGenericArrays = nullptr, size_t numUga = 0)
  {
    std::vector<unsigned int>& indexArray = tempArrays.IndexArray;
    std::vector<unsigned int>& indexToCell = tempArrays.IndexToCell;
    std::vector<int>& vertexToCell = tempArrays.VertexToCell;
    std::vector<float>& perPrimNormal = tempArrays.PerPrimNormal;
    std::vector<unsigned char>& perPrimColor = tempArrays.PerPrimColor;

    const int numPrimIdx = geomType + 1;

    // Establish vertex/cell normals
    useCellNormals = prop->GetInterpolation() == VTK_FLAT;

    // If no normals available, or winding order has to be consistent, regenerate polydata
    if (geomType == 2 &&
      normalGenerator != nullptr &&
      (!GetPolyDataNormals(polyData, useCellNormals) || forceConsistentWinding))
    {
      normalGenerator->SetComputePointNormals(!useCellNormals);
      normalGenerator->SetComputeCellNormals(useCellNormals);
      normalGenerator->SetConsistency(forceConsistentWinding);
      normalGenerator->SetInputData(polyData);

      normalGenerator->Update();

      polyData = normalGenerator->GetOutput();

      // Fix up the data pointers used by the generic array update records
      vtkFieldData* pointData = polyData->GetPointData();
      vtkFieldData* cellData = polyData->GetCellData();

      FixUpdatedGenericArrays(updatedGenericArrays, numUga, pointData, false);
      FixUpdatedGenericArrays(updatedGenericArrays, numUga, cellData, true);
    }

    // Points
    points = polyData->GetPoints()->GetData();
    
    // Establish vertex/cell colors
    useCellColors = false;
    colors = GetColorsAndInterpolation(mapper, polyData, useCellColors);

    // Generate the index arrays.
    numVerts = (size_t)polyData->GetPoints()->GetNumberOfPoints();
    size_t numPrims = numVerts;

    if (geomType != 0)
    {
      if (useCellNormals || useCellColors || tempArrays.HasPerCellGenericArrays())
      {
        MakeConnectivity(polyData, representation, numVerts, indexArray, indexToCell, nullptr, geomType, stickLinesEnabled, stickWireframeEnabled, isSticks);
        assert(indexArray.size() == indexToCell.size());
      }
      else
      {
        //indexToCell does not have to be generated
        EmptyVector<unsigned int> emptyVec;
        MakeConnectivity(polyData, representation, numVerts, indexArray, emptyVec, nullptr, geomType, stickLinesEnabled, stickWireframeEnabled, isSticks);
      }

      assert((indexArray.size() % numPrimIdx) == 0); // Index array size corresponds with primitive type
      numPrims = (indexArray.size() / numPrimIdx);
    }
    else
    {
      //Connectivity information
      EmptyVector<unsigned int> emptyVec;
      MakeConnectivity(polyData, representation, numVerts, indexArray, emptyVec, &vertexToCell, 0);

      //Vertex from point array does not necessarily have to belong to a cell visited by makeconnectivity
      //The index array is still empty, only vertexToCell is filled.
      //
      //All vertices of instancerData.Points will be rendered by default, so we only have to figure out which vertices
      //are invisible. Indices to those vertices that are invisible are therefore collected into the index array.
      for (int i = 0; i < vertexToCell.size(); ++i)
      {
        if (vertexToCell[i] == -1)
        {
          indexArray.push_back(i);
        }
      }
    }

    // Normals
    normals = nullptr;
    if (geomType == 2) // Only makes sense for triangle geometry (other geometries attach different meanings)
    {
      normals = GetPolyDataNormals(polyData, useCellNormals);

      const int numNormalComps = 3;
      if (normals != nullptr && normals->GetNumberOfComponents() != numNormalComps)
      {
        vtkGenericWarningMacro("Only 3-component normals supported");
        normals = nullptr;
      }

      // Fill perPrimNormal array in case of cellnormals
      if (normals != nullptr)
      {
        if (useCellNormals)
        {
          perPrimNormal.resize(numPrims * numNormalComps);
          float* src = normals->GetPointer(0);
          float* dest = &perPrimNormal[0];
          for (int i = 0; i < numPrims; ++i)
          {
            int cellIdx = indexToCell[i * numPrimIdx];
            assert(cellIdx < normals->GetNumberOfTuples());
            memcpy(dest + i * numNormalComps, src + cellIdx * numNormalComps, sizeof(float) * numNormalComps);
          }
        }
        else
        {
          assert((uint64_t)normals->GetNumberOfTuples() == numVerts);
        }
      }
    }

    // Copy colors
    if (colors != nullptr)
    {
      if (useCellColors)
      {
        int numSrcComps = colors->GetNumberOfComponents();
        perPrimColor.resize(numPrims * numSrcComps); //rgba bytes for every triangle

        for (int i = 0; i < numPrims; ++i)
        {
          int cellIdx = indexToCell[i * numPrimIdx];
          if (cellIdx != -1) // Vertex from point array does not necessarily have to belong to a cell visited by makeconnectivity.
          {
            unsigned char* dest = &perPrimColor[i * numSrcComps];
            assert(cellIdx < colors->GetNumberOfTuples());
            unsigned char* src = colors->GetPointer(cellIdx * numSrcComps);
            memcpy(dest, src, numSrcComps);
          }
        }
      }
      else
      {
        assert((uint64_t)colors->GetNumberOfTuples() == numVerts);
      }
    }

    // Texture coordinates
    texcoords = GetTextureCoordinates(mapper, polyData, hasTexture);

    //Fields
    //vtkFieldData* fields = polyData->GetPointData();
    //for (int i = 0; i < fields->GetNumberOfArrays(); ++i)
    //{
    //  vtkDataArray* someArray = fields->GetArray(i);
    //  someArray = someArray;
    //}

#if VTK_MODULE_ENABLE_VTK_RenderingRayTracing
    //Scales
    if (geomType < 2)
    {
      int scalingMode = vtkOSPRayActorNode::GetEnableScaling(act);
      //Scales
      if (scalingMode > vtkOSPRayActorNode::ALL_APPROXIMATE)
      {
        vtkInformation* mapInfo = mapper->GetInformation();
        char* scaleArrayName = (char*)mapInfo->Get(vtkOSPRayActorNode::SCALE_ARRAY_NAME());
        // scalearray is already per-vertex, but may not contain final values if mapped through the scaleFunction.
        // This has to be performed for the geoms individually.
        scaleArray = polyData->GetPointData()->GetArray(scaleArrayName);
        assert(!scaleArray || scaleArray->GetNumberOfTuples() == numVerts);
        if (scalingMode != vtkOSPRayActorNode::EACH_EXACT)
        {
          scaleFunction = vtkPiecewiseFunction::SafeDownCast(mapInfo->Get(vtkOSPRayActorNode::SCALE_FUNCTION()));
        }
      }
    }
#endif
  }

  void ConvertLinesToSticks(vtkOmniConnectTempArrays& tempArrays, vtkDataArray* points, vtkUnsignedCharArray* colors, vtkDataArray* texcoords, vtkDataArray* scaleArray, vtkPiecewiseFunction* scaleFunction,
    float uniformWidth, bool useCellColors)
  {
    auto& indices = tempArrays.IndexArray;
    assert((indices.size() % 2) == 0);
    size_t numLines = indices.size() / 2;
    tempArrays.PointsArray.resize(numLines*3);
    tempArrays.ScalesArray.resize(numLines * 3); // Scales are always present
    tempArrays.OrientationsArray.resize(numLines * 4);
    if (colors)
    {
      tempArrays.ColorsArray.resize(numLines*4);
    }
    if (texcoords)
    {
      tempArrays.TexCoordsArray.resize(numLines*2);
    }
    size_t ugaLen = tempArrays.SyncNumGenericArrays();
    for(int ugaIdx = 0; ugaIdx < ugaLen; ++ugaIdx)
    {
      tempArrays.ResetGenericArray(ugaIdx, numLines);
    }

    for (size_t i = 0; i < indices.size(); i+=2)
    {
      size_t primIdx = i / 2;
      auto vertIdx0 = indices[i];
      auto vertIdx1 = indices[i+1];

      float point0[3], point1[3];
      double point0d[3], point1d[3];
      points->GetTuple(vertIdx0, point0d);
      points->GetTuple(vertIdx1, point1d);
      point0[0] = (float)point0d[0];
      point0[1] = (float)point0d[1];
      point0[2] = (float)point0d[2];
      point1[0] = (float)point1d[0];
      point1[1] = (float)point1d[1];
      point1[2] = (float)point1d[2];

      tempArrays.PointsArray[primIdx * 3] = (point0[0] + point1[0]) * 0.5f;
      tempArrays.PointsArray[primIdx * 3 + 1] = (point0[1] + point1[1]) * 0.5f;
      tempArrays.PointsArray[primIdx * 3 + 2] = (point0[2] + point1[2]) * 0.5f;

      double scaleVal = uniformWidth;
      if (scaleArray)
      {
        scaleVal = *scaleArray->GetTuple(vertIdx0);
        if (scaleFunction != nullptr)
          scaleVal = scaleFunction->GetValue(scaleVal);
      }

      float segDir[3] = {
        point1[0] - point0[0],
        point1[1] - point0[1],
        point1[2] - point0[2],
      };
      float segLength = vtkMath::Norm(segDir);
      tempArrays.ScalesArray[primIdx * 3] = scaleVal;
      tempArrays.ScalesArray[primIdx * 3 + 1] = segLength;
      tempArrays.ScalesArray[primIdx * 3 + 2] = scaleVal;

      // Rotation

      DirectionToQuaternionY(segDir, segLength, &tempArrays.OrientationsArray[primIdx * 4]);

      //Colors

      if (colors)
      {
        bool hasAlpha = colors->GetNumberOfComponents() > 3;
        if (useCellColors)
        {
          size_t baseIdx = primIdx * 4;
          assert(baseIdx < tempArrays.PerPrimColor.size());
          tempArrays.ColorsArray[primIdx * 4] = tempArrays.PerPrimColor[baseIdx];
          tempArrays.ColorsArray[primIdx * 4 + 1] = tempArrays.PerPrimColor[baseIdx + 1];
          tempArrays.ColorsArray[primIdx * 4 + 2] = tempArrays.PerPrimColor[baseIdx + 2];
          tempArrays.ColorsArray[primIdx * 4 + 3] = hasAlpha ? tempArrays.PerPrimColor[baseIdx + 3] : 255;
        }
        else
        {
          assert(vertIdx0 < colors->GetNumberOfTuples());
          size_t baseIdx = vertIdx0 * 4;
          tempArrays.ColorsArray[primIdx * 4] = colors->GetValue(baseIdx);
          tempArrays.ColorsArray[primIdx * 4 + 1] = colors->GetValue(baseIdx + 1);
          tempArrays.ColorsArray[primIdx * 4 + 2] = colors->GetValue(baseIdx + 2);
          tempArrays.ColorsArray[primIdx * 4 + 3] = hasAlpha ? colors->GetValue(baseIdx + 3) : 255;
        }
      }

      // Texcoords

      if (texcoords)
      {
        assert(vertIdx0 < texcoords->GetNumberOfTuples());
        double srcTexCoord[2];
        texcoords->GetTuple(vertIdx0, srcTexCoord);
        tempArrays.TexCoordsArray[primIdx * 2] = srcTexCoord[0];
        tempArrays.TexCoordsArray[primIdx * 2 + 1] = srcTexCoord[1];
      }

      // Generic Arrays
      for(int ugaIdx = 0; ugaIdx < ugaLen; ++ugaIdx)
      {
        size_t srcIdx = tempArrays.UpdatedGenericArrays[ugaIdx].PerPoly ? tempArrays.IndexToCell[i] : indices[i];
        tempArrays.CopyToGenericArray(ugaIdx, srcIdx, primIdx, 1);
      }
    }

  }

  void ReorderCurveGeometry(vtkOmniConnectTempArrays& tempArrays, vtkDataArray* points, vtkUnsignedCharArray* colors, vtkDataArray* texcoords, vtkDataArray* scaleArray, vtkPiecewiseFunction* scaleFunction,
    bool useCellColors)
  {
    // Create curves
    auto& indices = tempArrays.IndexArray;
    assert((indices.size() % 2) == 0);
    size_t maxNumVerts = indices.size();
    tempArrays.CurveLengths.resize(0);
    tempArrays.PointsArray.resize(0);
    tempArrays.PointsArray.reserve(maxNumVerts *3); // Conservative max number of points
    if (colors)
    {
      tempArrays.ColorsArray.resize(0);
      tempArrays.ColorsArray.reserve(maxNumVerts *4);
    }
    if (texcoords)
    {
      tempArrays.TexCoordsArray.resize(0);
      tempArrays.TexCoordsArray.reserve(maxNumVerts *2);
    }
    if (scaleArray)
    {
      tempArrays.ScalesArray.resize(0);
      tempArrays.ScalesArray.reserve(maxNumVerts);
    }
    size_t ugaLen = tempArrays.SyncNumGenericArrays();
    for(int ugaIdx = 0; ugaIdx < ugaLen; ++ugaIdx)
    {
      tempArrays.ResetGenericArray(ugaIdx, 0);
      tempArrays.ReserveGenericArray(ugaIdx, maxNumVerts);
    }
    bool perCellGenericArrays = tempArrays.HasPerCellGenericArrays();

    int curveLength = 0;
    for (size_t i = 0; i < indices.size(); ++i)
    {
      bool repeatIndex = (i != 0) && indices[i] == indices[i - 1] && (!perCellGenericArrays || tempArrays.IndexToCell[i] == tempArrays.IndexToCell[i-1]); // Repeated if indices and cell (if available) are the same
      if (!repeatIndex)
      {
        if (((i % 2) == 0) && curveLength != 0) // Start of nonrepeated line segment with nonzero curvelength; store previous curve
        {
          tempArrays.CurveLengths.push_back(curveLength);
          curveLength = 0;
        }

        size_t primIdx = i / 2;
        auto vertIdx = indices[i];

        assert(vertIdx < points->GetNumberOfTuples());
        double srcPoint[3];
        points->GetTuple(vertIdx, srcPoint);
        tempArrays.PointsArray.push_back(srcPoint[0]);
        tempArrays.PointsArray.push_back(srcPoint[1]);
        tempArrays.PointsArray.push_back(srcPoint[2]);

        if (colors)
        {
          bool hasAlpha = colors->GetNumberOfComponents() > 3;
          if (useCellColors)
          {
            size_t baseIdx = primIdx * 4;
            assert(baseIdx < tempArrays.PerPrimColor.size());
            tempArrays.ColorsArray.push_back(tempArrays.PerPrimColor[baseIdx]);
            tempArrays.ColorsArray.push_back(tempArrays.PerPrimColor[baseIdx + 1]);
            tempArrays.ColorsArray.push_back(tempArrays.PerPrimColor[baseIdx + 2]);
            tempArrays.ColorsArray.push_back(hasAlpha ? tempArrays.PerPrimColor[baseIdx + 3] : 255);
          }
          else
          {
            assert(vertIdx < colors->GetNumberOfTuples());
            size_t baseIdx = vertIdx * 4;
            tempArrays.ColorsArray.push_back(colors->GetValue(baseIdx));
            tempArrays.ColorsArray.push_back(colors->GetValue(baseIdx + 1));
            tempArrays.ColorsArray.push_back(colors->GetValue(baseIdx + 2));
            tempArrays.ColorsArray.push_back(hasAlpha ? colors->GetValue(baseIdx + 3) : 255);
          }
        }

        if (texcoords)
        {
          assert(vertIdx < texcoords->GetNumberOfTuples());
          double srcTexCoord[2];
          texcoords->GetTuple(vertIdx, srcTexCoord);
          tempArrays.TexCoordsArray.push_back(srcTexCoord[0]);
          tempArrays.TexCoordsArray.push_back(srcTexCoord[1]);
        }

        if (scaleArray)
        {
          double scaleVal = *scaleArray->GetTuple(vertIdx);
          if (scaleFunction != nullptr)
            scaleVal = scaleFunction->GetValue(scaleVal);
          tempArrays.ScalesArray.push_back(static_cast<float>(scaleVal));
        }

        for(int ugaIdx = 0; ugaIdx < ugaLen; ++ugaIdx)
        {
          if (!tempArrays.UpdatedGenericArrays[ugaIdx].PerPoly || curveLength == 0) // Perpoly arrays are only expanded upon encountering a new curve
          {
            size_t srcIdx = tempArrays.UpdatedGenericArrays[ugaIdx].PerPoly ? tempArrays.IndexToCell[i] : vertIdx;
            size_t dstIdx = tempArrays.ExpandGenericArray(ugaIdx, 1);
            tempArrays.CopyToGenericArray(ugaIdx, srcIdx, dstIdx, 1);
          }
        }

        curveLength += 1;
      }
    }
    if (curveLength != 0) // store remaining curve
      tempArrays.CurveLengths.push_back(curveLength);
  }

  void GatherPointData(OmniConnectInstancerData& instancerData, vtkOmniConnectTempArrays& tempArrays,
    vtkMapper* mapper, vtkProperty* prop, vtkPolyData* polyData, int representation, bool hasTexture)
  {
    bool useCellNormals = false;
    bool useCellColors = false;
    bool forceConsistentWinding = false;
    size_t numVerts = 0;
    vtkDataArray* points = nullptr;
    vtkFloatArray* normals = nullptr;
    vtkUnsignedCharArray* colors = nullptr;
    vtkDataArray* texcoords = nullptr;
    vtkDataArray* scaleArray = nullptr;
    vtkPiecewiseFunction* scaleFunction = nullptr;

    GatherConnectedGeom(numVerts, points, normals, colors, texcoords, scaleArray, scaleFunction,
      useCellNormals, useCellColors, forceConsistentWinding,
      tempArrays, mapper, prop, polyData, representation, nullptr, hasTexture,
      0);

    instancerData.NumPoints = numVerts;
    instancerData.Points = points->GetVoidPointer(0);
    instancerData.PointsType = GetOmniConnectType(points);

    uint64_t numInvisIdx = tempArrays.IndexArray.size(); // Should only contain those vertices whose indices don't belong to any cell
    instancerData.NumInvisibleIndices = numInvisIdx;
    tempArrays.InvisibleIndicesArray.assign(tempArrays.IndexArray.begin(), tempArrays.IndexArray.end());
    if (numInvisIdx)
      instancerData.InvisibleIndices = tempArrays.InvisibleIndicesArray.data();

    // Copy Texcoords
    if (texcoords != nullptr)
    {
      instancerData.TexCoords = texcoords->GetVoidPointer(0);
      instancerData.TexCoordsType = GetOmniConnectType(texcoords);
    }

    // Copy colors
    if (colors != nullptr)
    {
      if (useCellColors)
        instancerData.Colors = &tempArrays.PerPrimColor[0];
      else
        instancerData.Colors = (unsigned char *)colors->GetVoidPointer(0);
      instancerData.ColorComponents = colors->GetNumberOfComponents();
    }

    // Assign scales
    double length = 1.0;
    length = mapper->GetLength();
    double pointSize = length / 1000.0 * prop->GetPointSize();
    instancerData.UniformScale = pointSize;

    if (scaleArray != nullptr)
    {
      if (scaleFunction != nullptr)
      {
        MapScalesThroughPWF(scaleArray, scaleFunction, tempArrays);
        instancerData.Scales = &tempArrays.ScalesArray[0];
        instancerData.ScalesType = OmniConnectType::FLOAT;
      }
      else
      {
        instancerData.Scales = scaleArray->GetVoidPointer(0);
        instancerData.ScalesType = GetOmniConnectType(scaleArray);
      }
    }
  }

  void GatherStickData(OmniConnectInstancerData& instancerData, vtkOmniConnectTempArrays& tempArrays,
    vtkMapper* mapper, vtkProperty* prop, vtkPolyData* polyData, int representation, bool hasTexture, bool stickLinesEnabled, bool stickWireframeEnabled)
  {
    bool useCellNormals = false;
    bool useCellColors = false;
    bool forceConsistentWinding = false;
    size_t numVerts = 0;
    vtkDataArray* points = nullptr;
    vtkFloatArray* normals = nullptr;
    vtkUnsignedCharArray* colors = nullptr;
    vtkDataArray* texcoords = nullptr;
    vtkDataArray* scaleArray = nullptr;
    vtkPiecewiseFunction* scaleFunction = nullptr;

    GatherConnectedGeom(numVerts, points, normals, colors, texcoords, scaleArray, scaleFunction,
      useCellNormals, useCellColors, forceConsistentWinding,
      tempArrays, mapper, prop, polyData, representation, nullptr, hasTexture,
      1, stickLinesEnabled, stickWireframeEnabled, true);

    double length = 1.0;
    length = mapper->GetLength();
    double lineWidth = length / 1000.0 * prop->GetLineWidth();

    ConvertLinesToSticks(tempArrays, points, colors, texcoords, scaleArray, scaleFunction, lineWidth, useCellColors);

    instancerData.PointsType = OmniConnectType::FLOAT3;
    instancerData.NumPoints = tempArrays.PointsArray.size() / 3;
    if (instancerData.NumPoints > 0)
    {
      instancerData.Points = &tempArrays.PointsArray[0];

      // Copy Texcoords
      if (texcoords != nullptr)
      {
        instancerData.TexCoords = &tempArrays.TexCoordsArray[0];
        instancerData.TexCoordsType = OmniConnectType::FLOAT2;
      }

      // Copy colors
      if (colors != nullptr)
      {
        instancerData.Colors = &tempArrays.ColorsArray[0];
        instancerData.ColorComponents = 4;
      }

      // Assign scales
      instancerData.Scales = &tempArrays.ScalesArray[0];
      instancerData.ScalesType = OmniConnectType::FLOAT3;

      // Assign orientations
      instancerData.Orientations = &tempArrays.OrientationsArray[0];
      instancerData.OrientationsType = OmniConnectType::FLOAT4;

      // Assign generic Arrays
      tempArrays.AssignGenericArraysToUpdated(true);
    }
  }

  void GatherCurveData(OmniConnectCurveData& curveData, vtkOmniConnectTempArrays& tempArrays,
    vtkMapper* mapper, vtkProperty* prop, vtkPolyData* polyData, int representation, bool hasTexture, bool stickLinesEnabled, bool stickWireframeEnabled)
  {
    bool useCellNormals = false;
    bool useCellColors = false;
    bool forceConsistentWinding = false;
    size_t numVerts = 0;
    vtkDataArray* points = nullptr;
    vtkFloatArray* normals = nullptr;
    vtkUnsignedCharArray* colors = nullptr;
    vtkDataArray* texcoords = nullptr;
    vtkDataArray* scaleArray = nullptr;
    vtkPiecewiseFunction* scaleFunction = nullptr;

    GatherConnectedGeom(numVerts, points, normals, colors, texcoords, scaleArray, scaleFunction,
      useCellNormals, useCellColors, forceConsistentWinding,
      tempArrays, mapper, prop, polyData, representation, nullptr, hasTexture,
      1, stickLinesEnabled, stickWireframeEnabled, false);

    ReorderCurveGeometry(tempArrays, points, colors, texcoords, scaleArray, scaleFunction, useCellColors);

    curveData.PointsType = OmniConnectType::FLOAT3;
    curveData.NumPoints = tempArrays.PointsArray.size()/3;
    if (curveData.NumPoints > 0)
    {
      curveData.Points = &tempArrays.PointsArray[0];
      curveData.CurveLengths = &tempArrays.CurveLengths[0];
      curveData.NumCurveLengths = tempArrays.CurveLengths.size();

      // Normals for orientations still missing (not sure if PV contains this info)

      // Copy Texcoords
      if (texcoords != nullptr)
      {
        curveData.TexCoords = &tempArrays.TexCoordsArray[0];
        curveData.TexCoordsType = OmniConnectType::FLOAT2;
      }
      curveData.PerPrimTexCoords = false; // Always vertex colored as per ReorderCurveGeometry. One entry per whole curve would be useless

      // Copy colors
      if (colors != nullptr)
      {
        curveData.Colors = &tempArrays.ColorsArray[0];
        curveData.ColorComponents = 4;
      }
      curveData.PerPrimColors = false; // Always vertex colored as per ReorderCurveGeometry. One entry per whole curve would be useless

      // Assign scales
      double length = 1.0;
      length = mapper->GetLength();
      double lineWidth = length / 1000.0 * prop->GetLineWidth();
      curveData.UniformScale = lineWidth;

      if (scaleArray != nullptr)
      {
        curveData.Scales = &tempArrays.ScalesArray[0];
        curveData.ScalesType = OmniConnectType::FLOAT;
      }

      // Assign generic Arrays
      tempArrays.AssignGenericArraysToUpdated(false);
    }
  }

  void GatherMeshData(OmniConnectMeshData& meshData, vtkOmniConnectTempArrays& tempArrays,
    vtkMapper* mapper, vtkProperty* prop, vtkPolyData* polyData, int representation, vtkPolyDataNormals* normalGenerator,
    bool hasTexture, bool forceConsistentWinding,
    OmniConnectGenericArray* updatedGenericArrays, size_t numUga)
  {
    bool useCellNormals = false;
    bool useCellColors = false;
    size_t numVerts = 0;
    vtkDataArray* points = nullptr;
    vtkFloatArray* normals = nullptr;
    vtkUnsignedCharArray* colors = nullptr;
    vtkDataArray* texcoords = nullptr;
    vtkDataArray* scaleArray = nullptr;
    vtkPiecewiseFunction* scaleFunction = nullptr;

    GatherConnectedGeom(numVerts, points, normals, colors, texcoords, scaleArray, scaleFunction,
      useCellNormals, useCellColors, forceConsistentWinding,
      tempArrays, mapper, prop, polyData, representation, normalGenerator, hasTexture, 2,
      false, false, false, updatedGenericArrays, numUga);

    //Copy to points to meshdata
    meshData.NumPoints = numVerts;
    meshData.Points = points->GetVoidPointer(0);
    meshData.PointsType = GetOmniConnectType(points);

    // Normals to meshData
    if (normals != nullptr)
    {
      if (useCellNormals)
      {
        meshData.Normals = &tempArrays.PerPrimNormal[0];
        meshData.NormalsType = OmniConnectType::FLOAT3;
      }
      else
      {
        meshData.Normals = normals->GetVoidPointer(0);
        meshData.NormalsType = GetOmniConnectType(normals);
      }
      meshData.PerPrimNormals = useCellNormals;
    }

    // Copy Texcoords
    if (texcoords != nullptr)
    {
      meshData.TexCoords = texcoords->GetVoidPointer(0);
      meshData.TexCoordsType = GetOmniConnectType(texcoords);
    }

    // Copy colors
    if (colors != nullptr)
    {
      if (useCellColors)
        meshData.Colors = &tempArrays.PerPrimColor[0];
      else
        meshData.Colors = (unsigned char *)colors->GetVoidPointer(0);
      meshData.ColorComponents = colors->GetNumberOfComponents();
    }
    meshData.PerPrimColors = useCellColors;

    // Copy indices
    meshData.NumIndices = tempArrays.IndexArray.size();
    if (meshData.NumIndices > 0)
    {
      meshData.Indices = &tempArrays.IndexArray[0];
      //meshData.FaceVertCounts = &faceVertCounts[0];
      //meshData.NumFaceVertCounts = faceVertCounts.size();
    }
  }

  void ExtractGeomTypes(vtkPolyData* poly, int representation, bool stickLinesEnabled, bool stickWireframeEnabled,
    bool& hasPointGeom, bool& hasStickGeom, bool& hasLineGeom, bool& hasTriangleGeom)
  {
    vtkCellArray *prims[4];
    prims[0] = poly->GetVerts();
    prims[1] = poly->GetLines();
    prims[2] = poly->GetPolys();
    prims[3] = poly->GetStrips();

    hasPointGeom = false;
    hasStickGeom = false;
    hasLineGeom = false;
    hasTriangleGeom = false;

    switch (representation)
    {
    case VTK_POINTS:
    {
      if (prims[0]->GetNumberOfCells() || prims[1]->GetNumberOfCells() || prims[2]->GetNumberOfCells() || prims[3]->GetNumberOfCells())
        hasPointGeom = true;
      break;
    }
    case VTK_WIREFRAME:
    {
      if (prims[0]->GetNumberOfCells())
        hasPointGeom = true;
      if (prims[1]->GetNumberOfCells())
      {
        if (stickLinesEnabled)
          hasStickGeom = true;
        else
          hasLineGeom = true;
      }
      if (prims[2]->GetNumberOfCells() || prims[3]->GetNumberOfCells())
      {
        if (stickWireframeEnabled)
          hasStickGeom = true;
        else
          hasLineGeom = true;
      }
      break;
    }
    default:
    {
      if (prims[0]->GetNumberOfCells())
        hasPointGeom = true;
      if (prims[1]->GetNumberOfCells() )
      {
        if (stickLinesEnabled)
          hasStickGeom = true;
        else
          hasLineGeom = true;
      }
      if (prims[2]->GetNumberOfCells() || prims[3]->GetNumberOfCells())
        hasTriangleGeom = true;
      break;
    }
    }
  }
}


//============================================================================
class vtkOmniConnectPolyDataMapperNodeInternals
{
public:
  vtkOmniConnectPolyDataMapperNodeInternals()
  {
    TempMappedTexture = vtkImageData::New();
  }

  ~vtkOmniConnectPolyDataMapperNodeInternals()
  {
    TempMappedTexture->Delete();
  }

  void ResetTempMappedTexture(vtkImageData* mappingInput, vtkUnsignedCharArray* mappedColors)
  {
    int mappedTexExtent[6] = { 0 };
    int mappedInputDims[3];

    mappingInput->GetDimensions(mappedInputDims);
    int* mappedInputExtent = mappingInput->GetExtent();

    // Switch around the extent coordinates if necessary, see vtkOpenGlTexture::Load()
    if (mappedInputDims[0] == 1)
    {
      // x := y
      mappedTexExtent[0] = mappedInputExtent[2];
      mappedTexExtent[1] = mappedInputExtent[3];
      // y := z
      mappedTexExtent[2] = mappedInputExtent[4];
      mappedTexExtent[3] = mappedInputExtent[5];
    }
    else
    {
      // x := x
      mappedTexExtent[0] = mappedInputExtent[0];
      mappedTexExtent[1] = mappedInputExtent[1];
      if (mappedInputDims[1] == 1)
      {
        // y := z
        mappedTexExtent[2] = mappedInputExtent[4];
        mappedTexExtent[3] = mappedInputExtent[5];
      }
      else
      {
        // y := y
        mappedTexExtent[2] = mappedInputExtent[2];
        mappedTexExtent[3] = mappedInputExtent[3];
        if (mappedInputDims[2] != 1)
        {
          vtkGenericWarningMacro("Only 2-dimensional textures are supported for mapping");
        }
      }
    }

    // Replace the extent+data of the temp mapped texture
    TempMappedTexture->SetExtent(mappedTexExtent);
    TempMappedTexture->GetPointData()->SetScalars(mappedColors);
  }

  bool IsColorMapped(vtkMapper* mapper, vtkPolyData* poly)
  {
    // Follows MapScalars and the subsequent color map creation in MapScalarsToTexture
    // Since ColorTextureMap == nullptr is assumed, the only conditions that remain for ColorTextureMap creation, are valid scalars and CanUseTextureMapForColoring().
    int cellFlag = -1;
    vtkAbstractArray* scalars = vtkAbstractMapper::GetAbstractScalars(
      poly, mapper->GetScalarMode(), mapper->GetArrayAccessMode(), mapper->GetArrayId(), mapper->GetArrayName(), cellFlag);
    return scalars != nullptr && mapper->CanUseTextureMapForColoring(poly);
  }

  vtkImageData* FindTextureForPoly(vtkActor* act, vtkMapper* mapper, vtkPolyData* poly)
  {
    // MAPSCALARS CALL (efficiency)
    if (!MapScalarsCalled)
    {
      int cellFlag = -1;
      mapper->MapScalars(poly, 1.0, cellFlag);
      MapScalarsCalled = true;
    }
    //

    if (mapper->GetColorMapColors())
      return nullptr;

    vtkImageData* resultCtm = mapper ? mapper->GetColorTextureMap() : nullptr;
    vtkProperty* property = act->GetProperty();

    if (!resultCtm)
    {
      vtkTexture* tex = nullptr;
      if (property->GetInterpolation() == VTK_PBR)
        tex = property->GetTexture("albedoTex");
      else
        tex = act->GetTexture();

      if (tex)
      {
        if (tex->GetColorMode() == VTK_COLOR_MODE_MAP_SCALARS)
        {
          vtkImageData* mappingInput = tex->GetInput();
          if (mappingInput)
          {
            vtkUnsignedCharArray* mappedColors = tex->GetMappedScalars();
            if (!mappedColors)
            {
              tex->MapScalarsToColors(mappingInput->GetPointData()->GetScalars());
              mappedColors = tex->GetMappedScalars();
            }
            // recheck
            if (mappedColors)
            {
              ResetTempMappedTexture(mappingInput, mappedColors);
              resultCtm = TempMappedTexture;
            }
          }
        }
        else
        {
          resultCtm = tex->GetInput();
        }
      }
    }
    return resultCtm;
  }

  std::vector<size_t> MaterialIds;
  bool MapScalarsCalled = false;
  vtkImageData* TempMappedTexture;
};

//============================================================================
vtkStandardNewMacro(vtkOmniConnectPolyDataMapperNode);

//----------------------------------------------------------------------------
vtkOmniConnectPolyDataMapperNode::vtkOmniConnectPolyDataMapperNode()
  : Internals(new vtkOmniConnectPolyDataMapperNodeInternals)
{
}

//----------------------------------------------------------------------------
vtkOmniConnectPolyDataMapperNode::~vtkOmniConnectPolyDataMapperNode()
{
  delete this->Internals;
  this->Internals = nullptr;
}

//----------------------------------------------------------------------------
void vtkOmniConnectPolyDataMapperNode::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
int vtkOmniConnectPolyDataMapperNode::GetRepresentation(vtkActor* actor)
{ 
  return actor->GetProperty()->GetRepresentation(); 
}

//----------------------------------------------------------------------------
void vtkOmniConnectPolyDataMapperNode::ResetMaterialIds()
{
  // Reset the visited material temp array
  this->Internals->MaterialIds.resize(0);
}

//----------------------------------------------------------------------------
void vtkOmniConnectPolyDataMapperNode::ExtractMapperProperties(vtkMapper* mapper)
{
  // Collection of mapper-level computations that are best performed only once per mapper node.
  this->HasTranslucentGeometry = mapper->HasTranslucentPolygonalGeometry();
}

//----------------------------------------------------------------------------
void vtkOmniConnectPolyDataMapperNode::RenderPoly(vtkOmniConnectRendererNode* rNode, vtkOmniConnectActorNodeBase* aNode, vtkActor* actor, vtkMapper* mapper, vtkPolyData* polyData, vtkProperty* prop,
  size_t dataEntryId, size_t materialId,
  double* overrideAmbientColor, double* overrideDiffuseColor, double* overrideSpecularColor, double overrideOpacity)
{
  int representation = this->GetRepresentation(actor);

  // Current solution for reserving two materials; one for polydatas that have colormapping (ie. a scalar array) and another (possibly regularly textured) when not
  materialId *= 2;
  // Establish whether we're dealing with a colormapped material or not, without actually mapping any scalars. Set material id accordingly.
  bool isColorMapped = this->Internals->IsColorMapped(mapper, polyData);
  if (isColorMapped)
    materialId += 1;

  OmniConnect* connector = rNode->GetOmniConnector();
  size_t actorId = aNode->GetActorId();
  double animTimeStep = actor->GetPropertyKeys() ? actor->GetPropertyKeys()->Get(vtkDataObject::DATA_TIME_STEP()) : 0.0f;
  this->Internals->MapScalarsCalled = false;

  aNode->SetSubGeomProgress(0.05);

  //
  // Create/update material entry in cache and send to connector
  //
  if (aNode->GetMaterialsChanged())
  {
    // Whether the material has been processed during this mapper pass (since ResetMaterialIds)
    // Note that in PV, textures cannot differ between polydatas, only texcoords. So they cannot be a source of different material ids.
    // Vertex colors are not purely down to the mapper, see DetermineVertexColors(), but we will assume they are for simplicity.
    auto it = std::find(this->Internals->MaterialIds.begin(), this->Internals->MaterialIds.end(), materialId);
    bool materialProcessed = (it != this->Internals->MaterialIds.end());

    if (!materialProcessed)
    {
      vtkOmniConnectImageWriter* imageWriter = rNode->GetImageWriter();

      // Update texture:
      // Pass on the texture data to the actor node, and update if necessary
      // Retrieve the texture cache in case it needs to be updated, and update immediately.
      vtkImageData* texData = this->Internals->FindTextureForPoly(actor, mapper, polyData);
      vtkOmniConnectTextureCache* texCache = nullptr;
      if (texData)
      {
        texCache = aNode->UpdateTextureCache(texData, materialId, mapper->GetColorTextureMap() == texData);
        aNode->SetSubGeomProgress(0.15);

        if (texCache->Status == OmniConnectStatusType::UPDATE)
        {
          // Data has to go through the image writer before it can be uploaded
          bool modifyTextureBorder = texCache->IsColorTextureMap;
          imageWriter->SetModifyBorder(modifyTextureBorder); //is a hack
          bool imageWritten = imageWriter->WriteData(texData);

          if (imageWritten)
          {
            char* imageData; int imageLength;
            imageWriter->GetResult(imageData, imageLength);

            OmniConnectSamplerData samplerData;
            samplerData.TexData = imageData;
            samplerData.TexDataSize = imageLength;
            samplerData.WrapS = samplerData.WrapT = texCache->IsColorTextureMap ? OmniConnectSamplerData::WrapMode::CLAMP : OmniConnectSamplerData::WrapMode::REPEAT;

            // Upload the texture image data
            connector->UpdateTexture(actorId, texCache->TexId, samplerData,
              rNode->GetForceTimeVaryingTextures(), animTimeStep);
          }
          else
            vtkOmniConnectLogCallback::Callback(OmniConnectLogLevel::ERR, nullptr, "Texture image could not be written");

          imageWriter->SetModifyBorder(false);
        }
        // Deletion of textures happens later on in vtkOmniConnectActorNodeBase
      }

      aNode->SetSubGeomProgress(0.15);

      // Update Material

      // This is the first time we encounter this material within this particular renderpass, so update the definition on the OmniConnect side.
      {
        OmniConnectMaterialData omniMatData;
        omniMatData.MaterialId = materialId;
        SetUpdatesToPerform(omniMatData, rNode->GetForceTimeVaryingMaterialParams());

        bool useVertexColors = DetermineVertexColors(mapper, polyData); // Material-polydata dependency for vertexcolors, uncertain whether this evaluation can differ across polydatas in practice.
        bool opacityMappingEnabled = !mapper->GetLookupTable()->IsOpaque();

        GatherMaterialData(prop, texData ? texCache->TexId : -1, omniMatData,
          overrideAmbientColor,
          overrideDiffuseColor,
          overrideSpecularColor,
          overrideOpacity,
          useVertexColors,
          this->HasTranslucentGeometry,
          opacityMappingEnabled
#if defined(USE_MDL_MATERIALS) && USE_CUSTOM_POINT_SHADER
          , !connector->GetSettings().UsePointInstancer &&
            this->PointsAreSpheres() &&
            (polyData->GetVerts()->GetNumberOfCells() || 
            (representation == VTK_POINTS ? 
              polyData->GetLines()->GetNumberOfCells() || polyData->GetPolys()->GetNumberOfCells() || polyData->GetStrips()->GetNumberOfCells() : false))
#endif
          );

        connector->UpdateMaterial(actorId, omniMatData, animTimeStep);
        this->Internals->MaterialIds.push_back(materialId);
      }
      // Materials are not deleted because they are not stored per-timestep and they have no means of identification across timesteps; as such they are simply overwritten by all new materials generated within this timestep update.
      // If a previous timestep update for this actor used more materials, the ones with higher indices simply remain and do no harm (with the added benefit that older timesteps can still be displayed).
    }
  }

  aNode->SetSubGeomProgress(0.3);

  //
  // Create/update the polydata with corresponding id in cache and send to connector
  //

  vtkOmniConnectTimeStep* omniTimeStep = aNode->GetTimeStep(animTimeStep);

  //Extract which dataEntryId/polydata pairs result in which geometries
  bool stickLinesEnabled = rNode->GetUseStickLines();
  bool stickWireframeEnabled = rNode->GetUseStickWireframe();

  bool hasTriGeom, hasLineGeom, hasStickGeom, hasPointGeom;
  ExtractGeomTypes(polyData, representation, stickLinesEnabled, stickWireframeEnabled,
    hasPointGeom, hasStickGeom, hasLineGeom, hasTriGeom);

  // When adding a geometry type, make sure that when OmniConnectGeomType is the same, the ids are different (therefore, lineGeomId/stickGeomId are offset)
  size_t meshGeomId = dataEntryId;
  size_t pointGeomId = dataEntryId;
  size_t lineGeomId = dataEntryId*2;
  size_t stickGeomId = dataEntryId*2+1;

  //Regardless of connector updating, the available geometries are tagged to exist for the actornode geom deletion
  bool newTriGeom = false, newLineGeom = false, newStickGeom = false, newPointGeom = false;
  if(hasTriGeom)
    newTriGeom = aNode->AddTransferredGeometry(meshGeomId, OmniConnectGeomType::MESH);
  if(hasLineGeom)
    newLineGeom = aNode->AddTransferredGeometry(lineGeomId, OmniConnectGeomType::CURVE);
  if (hasStickGeom)
    newStickGeom = aNode->AddTransferredGeometry(stickGeomId, OmniConnectGeomType::INSTANCER);
  if(hasPointGeom)
    newPointGeom = aNode->AddTransferredGeometry(pointGeomId, OmniConnectGeomType::INSTANCER);
  bool newGeom = newTriGeom || newLineGeom || newStickGeom || newPointGeom;

  bool forceGeomUpdate = rNode->GetPolyDataPolicyChanged() || aNode->GetMaterialsChanged() || this->HasNewShapeGeom();
  bool forceArrayUpdate = false;
  bool updatePolyData = omniTimeStep->UpdateDataEntry(polyData, dataEntryId, forceGeomUpdate, forceArrayUpdate);

  // Update double to float conversion setting specifically for the dataset currently processed
  SetConvertGenericArraysDoubleToFloat(connector, polyData, false);

  if (updatePolyData)
  {
    vtkOmniConnectTempArrays& tempArrays = rNode->GetTempArrays();
    bool representationChanged = aNode->GetRepresentationChanged();

    forceArrayUpdate = forceArrayUpdate || newGeom || representationChanged;

    // Extract updated and deleted generic arrays from vtk
    // GetUpdatedDeletedGenericArrays also disables generic arrays updates over timesteps in case of OmniConnectTemporalArraysFlag presence, but this should be disabled if geom (over all timesteps) or representation is new.
    omniTimeStep->GetUpdatedDeletedGenericArrays(dataEntryId,
      tempArrays.UpdatedGenericArrays,
      tempArrays.DeletedGenericArrays,
      forceArrayUpdate);

    aNode->SetSubGeomProgress(0.4);

    size_t ugaLen = tempArrays.UpdatedGenericArrays.size();
    size_t dgaLen = tempArrays.DeletedGenericArrays.size();
    OmniConnectGenericArray* updatedGenericArrays = tempArrays.UpdatedGenericArrays.data();
    OmniConnectGenericArray* deletedGenericArrays = tempArrays.DeletedGenericArrays.data();

    vtkImageData* texData = this->Internals->FindTextureForPoly(actor, mapper, polyData);

    // Extract the geometry data from vtk and upload to the connector along with generic data
    if (hasTriGeom)
    {
      OmniConnectMeshData omniMeshData;
      omniMeshData.MeshId = meshGeomId;
      SetUpdatesToPerform(omniMeshData, polyData, forceArrayUpdate); // Fill out omniMeshData.UpdatesToPerform: which standard arrays of the OmniConnectMeshData type to perform updates on (for manual disabling of standard array updates over timesteps).

      GatherMeshData(omniMeshData, tempArrays,
        mapper, prop, polyData, representation,
        rNode->GetNormalGenerator(), texData != nullptr, rNode->GetForceConsistentWinding(),
        updatedGenericArrays, ugaLen); // Normalgenerator may regenerate generic arrays

      connector->UpdateMesh(actorId, animTimeStep, omniMeshData, materialId,
        updatedGenericArrays, ugaLen,
        deletedGenericArrays, dgaLen);
      connector->SetGeomVisibility(actorId, omniMeshData.MeshId, OmniConnectGeomType::MESH, true, animTimeStep); //Always set geom to visible (geometry that became invisible handled in vtkOmniConnectActorNodeBase)
    }
    if (hasLineGeom)
    {
      OmniConnectCurveData omniCurveData;
      omniCurveData.CurveId = lineGeomId;
      SetUpdatesToPerform(omniCurveData, polyData, forceArrayUpdate);

      GatherCurveData(omniCurveData, tempArrays,
        mapper, prop, polyData, representation, texData != nullptr,
        stickLinesEnabled, stickWireframeEnabled);

      connector->UpdateCurve(actorId, animTimeStep, omniCurveData, materialId,
        updatedGenericArrays, ugaLen,
        deletedGenericArrays, dgaLen);
      connector->SetGeomVisibility(actorId, omniCurveData.CurveId, OmniConnectGeomType::CURVE, true, animTimeStep);
    }
    if (hasStickGeom)
    {
      OmniConnectInstancerData omniInstancerData;
      OmniConnectUpdateEvaluator<OmniConnectInstancerData> updateEval(omniInstancerData);
      omniInstancerData.InstancerId = stickGeomId;
      omniInstancerData.DefaultShape = OmniConnectInstancerData::SHAPE_CYLINDER;
      SetUpdatesToPerform(omniInstancerData, polyData, forceArrayUpdate);
      if (!newStickGeom)
        updateEval.RemoveUpdate(OmniConnectInstancerData::DataMemberId::SHAPES); //Shapes are set only at creation of an instancer

      GatherStickData(omniInstancerData, tempArrays,
        mapper, prop, polyData, representation, texData != nullptr,
        stickLinesEnabled, stickWireframeEnabled);

      connector->UpdateInstancer(actorId, animTimeStep, omniInstancerData, materialId,
        updatedGenericArrays, ugaLen,
        deletedGenericArrays, dgaLen);
      connector->SetGeomVisibility(actorId, omniInstancerData.InstancerId, OmniConnectGeomType::INSTANCER, true, animTimeStep);
    }
    if (hasPointGeom)
    {
      OmniConnectInstancerData omniInstancerData;
      OmniConnectUpdateEvaluator<OmniConnectInstancerData> updateEval(omniInstancerData);
      omniInstancerData.InstancerId = pointGeomId;
      SetUpdatesToPerform(omniInstancerData, polyData, forceArrayUpdate);
      if (!newPointGeom && !HasNewShapeGeom())
        updateEval.RemoveUpdate(OmniConnectInstancerData::DataMemberId::SHAPES); //Shapes are set only at creation of an instancer

      GatherPointData(omniInstancerData, tempArrays,
        mapper, prop, polyData, representation, texData != nullptr);

      GatherCustomPointAttributes(polyData, omniInstancerData);

      connector->UpdateInstancer(actorId, animTimeStep, omniInstancerData, materialId,
        updatedGenericArrays, ugaLen,
        deletedGenericArrays, dgaLen);
      connector->SetGeomVisibility(actorId, omniInstancerData.InstancerId, OmniConnectGeomType::INSTANCER, true, animTimeStep);
    }

    aNode->SetSubGeomProgress(0.9);

    // Cleanup generic array cache (to match arrays that were deleted in call to connector->UpdateMesh())
    omniTimeStep->CleanupGenericArrayCache(dataEntryId);
  }

}

//----------------------------------------------------------------------------
bool vtkOmniConnectPolyDataMapperNode::GetNodeNeedsProcessing(vtkOmniConnectRendererNode* rNode, vtkOmniConnectActorNodeBase* aNode,
  vtkOmniConnectTimeStep* omniTimeStep, vtkDataObject* poly, vtkCompositeDataDisplayAttributes* cda) const
{
  bool inputDataChanged = omniTimeStep->HasInputChanged(poly, cda);
  return rNode->GetPolyDataPolicyChanged() || aNode->GetMaterialsChanged() || this->HasNewShapeGeom() || inputDataChanged;
}

//----------------------------------------------------------------------------
void vtkOmniConnectPolyDataMapperNode::Build(bool prepass)
{
}

//----------------------------------------------------------------------------
void vtkOmniConnectPolyDataMapperNode::Render(bool prepass)
{
  if (prepass)
  {
    vtkOmniConnectRendererNode* rNode = vtkOmniConnectRendererNode::GetRendererNode(this);

    // we use a lot of params from our parent
    vtkOmniConnectActorNodeBase* aNode = &(vtkOmniConnectActorNode::SafeDownCast(this->Parent)->ActorNodeBase);
    vtkActor* actor = nullptr;

    // Guard against uninitialized actors or initialized ones that are invisible.
    if (!(actor = OmniConnectActorExistsAndInitialized<vtkActor>(aNode)))
      return;

    double animTimeStep = actor->GetPropertyKeys() ? actor->GetPropertyKeys()->Get(vtkDataObject::DATA_TIME_STEP()) : 0.0f;
    vtkOmniConnectTimeStep* omniTimeStep = aNode->GetTimeStep(animTimeStep);

    // Prepare vtk dataentry cache for node rebuild (set to remove unless explicitly defined otherwise).
    omniTimeStep->ResetDataEntryCache();
    this->ResetMaterialIds();

    // Find the polydata via the mapper
    vtkPolyData* poly = nullptr;
    vtkMapper* mapper = vtkMapper::SafeDownCast(this->GetRenderable());
    if (mapper && mapper->GetNumberOfInputPorts() > 0)
    {
      poly = vtkPolyData::SafeDownCast(mapper->GetInput());
    }
    if (poly)
    {
      aNode->SetSubGeomProgress(0.0);

      // Figure out whether any polydata changed
      if (!this->GetNodeNeedsProcessing(rNode, aNode, omniTimeStep, poly))
      {
        // Keep all vtk dataentry caches (dataEntryId)
        omniTimeStep->KeepDataEntryCache();
        // Make sure no earlier usd geometries are thrown out either (geomId)
        aNode->KeepTransferredGeometries();
      }
      else
      {
        aNode->SetInputDataChanged();

        if(aNode->GetMaterialsChanged())
          this->ExtractMapperProperties(mapper); // perform potentially expensive mapper computations only once after material change

        // If changes, convert the materials and geometry for this polydata
        vtkProperty* property = actor->GetProperty();
        this->RenderPoly(rNode, aNode, actor, mapper, poly, property, 0, 0,
          property->GetAmbientColor(), property->GetDiffuseColor(), property->GetSpecularColor(), property->GetOpacity());
      }

      aNode->SetSubGeomProgress(0.99);
    }

    // Clean up vtk dataentry cache
    omniTimeStep->CleanupDataEntryCache();
  }
}




