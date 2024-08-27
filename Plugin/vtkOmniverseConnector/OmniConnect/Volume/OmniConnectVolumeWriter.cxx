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

#include "OmniConnectVolumeWriter.h"
#include "OmniConnectUtilsInternal.h"

#include <memory>

class OmniConnectVolumeWriterInternals;

class OmniConnectVolumeWriter : public OmniConnectVolumeWriterI
{
  public:
    OmniConnectVolumeWriter();
    ~OmniConnectVolumeWriter();

    bool Initialize(OmniConnectLogCallback logCallback, void* logUserData) override;

    void ToVDB(const OmniConnectVolumeData& volumeData,
      OmniConnectGenericArray* updatedGenericArrays, size_t numUga) override;

    void GetSerializedVolumeData(const char*& data, size_t& size) override;

    void SetConvertDoubleToFloat(bool convert) override { ConvertDoubleToFloat = convert; }

    void Release() override;

    static OmniConnectLogCallback LogCallback;
    static void* LogUserData;

  protected:

    bool ConvertDoubleToFloat = true;

#ifdef USE_OPENVDB
    std::unique_ptr<OmniConnectVolumeWriterInternals> Internals;
#endif
};

extern "C" OmniConnectVolumeWriterI* OmniConnect_Vol_DECL Create_VolumeWriter()
{
  return new OmniConnectVolumeWriter();
}

void OmniConnectVolumeWriter::Release()
{
  delete this;
}


#ifdef USE_OPENVDB

// GridTransformer.h->LevelSetRebuild.h->MeshToVolume.h->tbb/enumerable_thread_specific.h->windows_api.h
//#ifdef _WIN32
//#define NOMINMAX
//#define _USE_MATH_DEFINES // for C
//#include <math.h>
//#endif

// To enable output that usdview can digest (just a single float)
//#define FLOAT1_OUTPUT

#include "openvdb/openvdb.h"
#include "openvdb/io/Stream.h"
#include "openvdb/tools/Dense.h"
#include "openvdb/tools/GridTransformer.h"
#include "openvdb/tree/ValueAccessor.h"

#include <assert.h>
#include <limits>
#include <sstream>

#define OmniConnectErrorMacro(x) \
  { std::stringstream logStream; \
    logStream << x; \
    std::string logString = logStream.str(); \
    OmniConnectVolumeWriter::LogCallback( OmniConnectLogLevel::ERR, nullptr, logString.c_str()); } 

#define OmniConnectDebugMacro(x) \
  { std::stringstream logStream; \
    logStream << x; \
    std::string logString = logStream.str(); \
    OmniConnectVolumeWriter::LogCallback( OmniConnectLogLevel::WARNING, nullptr, logString.c_str()); }  

OmniConnectLogCallback OmniConnectVolumeWriter::LogCallback = nullptr;
void* OmniConnectVolumeWriter::LogUserData = nullptr;

#ifdef FLOAT1_OUTPUT
using ColorGridOutType = openvdb::FloatGrid;
#else
using ColorGridOutType = openvdb::Vec3fGrid;
#endif
using OpacityGridOutType = openvdb::FloatGrid;

class OmniConnectVolumeWriterInternals
{
  public:
    OmniConnectVolumeWriterInternals()
      //: GridStream(std::ios::in | std::ios::out)
    {}
    ~OmniConnectVolumeWriterInternals()
    {
      delete GridStream;
    }

    std::ostream& ResetStream()
    {
      //GridStream.clear(); // Bug in OpenVDB prevents reuse of stream
      delete GridStream;
      GridStream = new std::stringstream(std::ios::in | std::ios::out);
      return *GridStream;
    }

    const char* GetStreamData()
    {
      if(!GridStream)
        return nullptr;

      StreamData = GridStream->str();
      return StreamData.c_str();
    }

    size_t GetStreamDataSize()
    {
      return StreamData.length();
    }

  protected:
    std::stringstream* GridStream = nullptr;
    std::string StreamData;
};

struct TfTransformInput
{
  ColorGridOutType::Ptr colorGrid;
  OpacityGridOutType::Ptr opacityGrid;
  const OmniConnectVolumeData& volumeData;
  const openvdb::CoordBBox& bBox;
};

template<typename DataType, typename OpType, typename TransformerType>
struct TfTransform
{
public:
  TfTransform(const OmniConnectVolumeData& volumeData, const openvdb::CoordBBox& bBox, const TransformerType& transformer)
    : Transformer(transformer)
    , VolData(static_cast<const DataType*>(volumeData.Data))
    , Dims(bBox.max() + openvdb::math::Coord(1, 1, 1)) //Bbox is inclusive, dims are exclusive
    , InvValueRangeMag(OpType(1.0) / (OpType)(volumeData.TfData.TfValueRange[1] - volumeData.TfData.TfValueRange[0]))
    , ValueRangeMin((OpType)(volumeData.TfData.TfValueRange[0]))
  {
  }

  inline void operator()(const typename TransformerType::Iter& iter) const
  {
    openvdb::math::Coord coord = iter.getCoord();

    assert(coord.x() >= 0 && coord.x() < Dims.x() &&
      coord.y() >= 0 && coord.y() < Dims.y() &&
      coord.z() >= 0 && coord.z() < Dims.z());
    size_t linearIndex = Dims.y() * Dims.x() * coord.z() + Dims.x() * coord.y() + coord.x();
    const DataType* curVal = VolData + linearIndex;

    OpType ucVal = (((OpType)(*curVal)) - this->ValueRangeMin) * this->InvValueRangeMag;
    float normValue = (float)((ucVal < (OpType)0.0) ? (OpType)0.0 : ((ucVal > (OpType)1.0) ? (OpType)1.0 : ucVal));

    Transformer.Transform(normValue, iter);
  }

  /*
  inline void operator()(
    const openvdb::FloatGrid::ValueOnCIter& iter,
    openvdb::tree::ValueAccessor<TreeOutType>& accessor)
  {
    openvdb::Vec4f transformedColor;
    transformColor(*iter, transformedColor);

    if (iter.isVoxelValue())
    { // set a single voxel
      accessor.setValue(iter.getCoord(), transformedColor);
    }
    else
    { // fill an entire tile
      openvdb::CoordBBox bbox;
      iter.getBoundingBox(bbox);
      accessor.getTree()->fill(bbox, transformedColor);
    }
  }
  */

  const TransformerType& Transformer;
  const DataType* VolData;
  openvdb::math::Coord Dims;
  OpType InvValueRangeMag;
  OpType ValueRangeMin;
};

struct TfColorTransformer
{
public:
  typedef ColorGridOutType::ValueOnIter Iter;

  TfColorTransformer(const OmniConnectVolumeData& volumeData)
    : TfColors(static_cast<const float*>(volumeData.TfData.TfColors))
    , NumTfColors(volumeData.TfData.TfNumValues)
  {                                               
  }

  inline void Transform(float normValue, const Iter& iter) const
  {
    openvdb::Vec3f transformedColor;

    float colorIndexF = normValue * (this->NumTfColors - 1);
    float floorColor;
    float fracColor = std::modf(colorIndexF, &floorColor);
    int colIdx0 = int(floorColor);
    int colIdx1 = colIdx0 + (colIdx0 != (this->NumTfColors - 1));

    const float* color0 = this->TfColors + 3 * colIdx0;
    const float* color1 = this->TfColors + 3 * colIdx1;
    float OneMinFracColor = 1.0f - fracColor;

    transformedColor[0] = fracColor * color1[0] + OneMinFracColor * color0[0];
    transformedColor[1] = fracColor * color1[1] + OneMinFracColor * color0[1];
    transformedColor[2] = fracColor * color1[2] + OneMinFracColor * color0[2];

#ifdef FLOAT1_OUTPUT
    iter.setValue(transformedColor.length());
#else
    iter.setValue(transformedColor);
#endif
  }
   
  const float* TfColors;
  int NumTfColors;
};

struct TfOpacityTransformer
{
public:
  typedef OpacityGridOutType::ValueOnIter Iter;

  TfOpacityTransformer(const OmniConnectVolumeData& volumeData)
    : TfOpacities(static_cast<const float*>(volumeData.TfData.TfOpacities))
    , NumTfOpacities(volumeData.TfData.TfNumValues)
  {
  }

  inline void Transform(float normValue, const Iter& iter) const
  {
    float opacityIndexF = normValue * (this->NumTfOpacities - 1);
    float floorOpacity;
    float fracOpacity = std::modf(opacityIndexF, &floorOpacity);
    int opacityIdx0 = int(floorOpacity);
    int opacityIdx1 = opacityIdx0 + (opacityIdx0 != (this->NumTfOpacities - 1));

    float finalOpacity = fracOpacity * this->TfOpacities[opacityIdx1] +
      (1.0f - fracOpacity) * this->TfOpacities[opacityIdx0];

    iter.setValue(finalOpacity);
  }

  const float* TfOpacities;
  int NumTfOpacities;
};

template<typename DataType, typename OpType>
void TfTransformCall(TfTransformInput& tfTransformInput)
{
  TfColorTransformer colorTransformer(tfTransformInput.volumeData);
  TfTransform<DataType, OpType, TfColorTransformer> tfColorTransform(tfTransformInput.volumeData, tfTransformInput.bBox, colorTransformer);
  openvdb::tools::foreach(tfTransformInput.colorGrid->beginValueOn(), tfColorTransform);
  
  TfOpacityTransformer opacityTransformer(tfTransformInput.volumeData);
  TfTransform<DataType, OpType, TfOpacityTransformer> tfOpacityTransform(tfTransformInput.volumeData, tfTransformInput.bBox, opacityTransformer);
  openvdb::tools::foreach(tfTransformInput.opacityGrid->beginValueOn(), tfOpacityTransform);
}

static void SelectTfTransform(TfTransformInput& tfTransformInput)
{
  // Transform the float data to color data
  switch (tfTransformInput.volumeData.DataType)
  {
  case OmniConnectType::CHAR:
    TfTransformCall<char, float>(tfTransformInput);
    break;
  case OmniConnectType::UCHAR:
    TfTransformCall<unsigned char, float>(tfTransformInput);
    break;
  case OmniConnectType::SHORT:
    TfTransformCall<short, float>(tfTransformInput);
    break;
  case OmniConnectType::USHORT:
    TfTransformCall<unsigned short, float>(tfTransformInput);
    break;
  case OmniConnectType::INT:
    TfTransformCall<int, double>(tfTransformInput);
    break;
  case OmniConnectType::UINT:
    TfTransformCall<unsigned int, double>(tfTransformInput);
    break;
  case OmniConnectType::LONG:
    TfTransformCall<long long, double>(tfTransformInput);
    break;
  case OmniConnectType::ULONG:
    TfTransformCall<unsigned long long, double>(tfTransformInput);
    break;
  case OmniConnectType::FLOAT:
    TfTransformCall<float, float>(tfTransformInput);
    break;
  case OmniConnectType::DOUBLE:
    TfTransformCall<double, double>(tfTransformInput);
    break;
  default:
    {
      const char* typeStr = ocutils::OmniConnectTypeToString(tfTransformInput.volumeData.DataType);
      OmniConnectErrorMacro("Volume writer preclassified copy does not support source data type: " << typeStr);
      break;
    }
  }
  //openvdb::tools::transformValues(valArray->cbeginValueOn(), *colorGrid, transformOp);
}


struct CopyToGridInput
{
  const OmniConnectVolumeData& volumeData;
  const openvdb::CoordBBox& bBox;
};

struct DoublePH // placeholder for double type to implicitly convert to float without warnings
{
  public:
    DoublePH(double val) : v(val) {}
    ~DoublePH() {}
    operator float() const { return static_cast<float>(v); }
  protected:
    double v;
};

template<typename InDataType, typename OutGridType>
struct GridConvert
{
  GridConvert(const OmniConnectVolumeData& volumeData, const openvdb::CoordBBox& bBox, bool normalize = true)
    : VolData(static_cast<const InDataType*>(volumeData.Data))
    , Dims(bBox.max() + openvdb::math::Coord(1, 1, 1)) //Bbox is inclusive, dims are exclusive
    , BackgroundIdx(volumeData.BackgroundIdx)
  {
  }

  inline void operator()(const typename OutGridType::ValueOnIter& iter) const
  {
    openvdb::math::Coord coord = iter.getCoord();

    assert(coord.x() >= 0 && coord.x() < Dims.x() &&
      coord.y() >= 0 && coord.y() < Dims.y() &&
      coord.z() >= 0 && coord.z() < Dims.z());
    size_t linearIndex = Dims.y() * Dims.x() * coord.z() + Dims.x() * coord.y() + coord.x();
    const InDataType* curVal = VolData + linearIndex;

    typename OutGridType::ValueType outVal(*curVal); 

    iter.setValue(outVal);
  }

  typename OutGridType::ValueType BackgroundValue()
  {
    if(BackgroundIdx == -1)
    {
      return typename OutGridType::ValueType(0);
    }

    const InDataType* backVal = VolData + BackgroundIdx;
    typename OutGridType::ValueType outVal(*backVal); 

    return outVal;
  }

  const InDataType* VolData;
  openvdb::math::Coord Dims;
  long long BackgroundIdx;
};

template<typename DataType>
struct NormalizedToGridConvert
{
  NormalizedToGridConvert(const OmniConnectVolumeData& volumeData, const openvdb::CoordBBox& bBox)
    : VolData(static_cast<const DataType*>(volumeData.Data))
    , Dims(bBox.max() + openvdb::math::Coord(1, 1, 1)) //Bbox is inclusive, dims are exclusive
    , MaxValue(static_cast<float>(std::numeric_limits<DataType>::max()))
    , MinValue(static_cast<float>(std::numeric_limits<DataType>::min()))
    , BackgroundIdx(volumeData.BackgroundIdx)
  {
    InvRange = 1.0f / (MaxValue - MinValue);
  }

  inline void operator()(const openvdb::FloatGrid::ValueOnIter& iter) const
  {
    openvdb::math::Coord coord = iter.getCoord();

    assert(coord.x() >= 0 && coord.x() < Dims.x() &&
      coord.y() >= 0 && coord.y() < Dims.y() &&
      coord.z() >= 0 && coord.z() < Dims.z());
    size_t linearIndex = Dims.y() * Dims.x() * coord.z() + Dims.x() * coord.y() + coord.x();
    const DataType* curVal = VolData + linearIndex;

    iter.setValue(((static_cast<float>(*curVal)) - MinValue) * InvRange);
  }

  float BackgroundValue()
  {
    return (BackgroundIdx == -1) ? 0.0f : 
      (static_cast<float>(*(VolData + BackgroundIdx)) - MinValue) * InvRange;
  }

  const DataType* VolData;
  openvdb::math::Coord Dims;
  float MaxValue;
  float MinValue;
  float InvRange;
  long long BackgroundIdx;
};

template<typename InDataType, typename OutGridType>
openvdb::GridBase::Ptr ConvertAndCopyToGridTemplate(const CopyToGridInput& copyInput)
{
  GridConvert<InDataType, OutGridType> gridConverter(copyInput.volumeData, copyInput.bBox);

  typename OutGridType::Ptr outGrid = OutGridType::create(gridConverter.BackgroundValue());
  typename OutGridType::ValueType defaultValue(0);
  outGrid->denseFill(copyInput.bBox, defaultValue, true);

  openvdb::tools::foreach(outGrid->beginValueOn(), gridConverter);

  return outGrid;
}

template<typename DataType>
openvdb::GridBase::Ptr NormalizedCopyToGridTemplate(const CopyToGridInput& copyInput)
{
  NormalizedToGridConvert<DataType> gridConverter(copyInput.volumeData, copyInput.bBox);

  openvdb::FloatGrid::Ptr floatGrid = openvdb::FloatGrid::create(gridConverter.BackgroundValue());
  floatGrid->denseFill(copyInput.bBox, 0.0f, true);

  openvdb::tools::foreach(floatGrid->beginValueOn(), gridConverter);

  return floatGrid;
}

template<typename DataType, typename GridType>
openvdb::GridBase::Ptr CopyToGridTemplate(const CopyToGridInput& copyInput)
{
  const DataType* typedData = static_cast<const DataType*>(copyInput.volumeData.Data);
  long long backgroundIdx = copyInput.volumeData.BackgroundIdx;
  typename GridType::ValueType backGroundValue( (backgroundIdx == -1) ? 
    static_cast<DataType>(0) : *(typedData + backgroundIdx)
    );

  typename GridType::Ptr scalarGrid = GridType::create(backGroundValue);

  openvdb::tools::Dense<const DataType, openvdb::tools::LayoutXYZ> valArray(copyInput.bBox, typedData);
  openvdb::tools::copyFromDense(valArray, *scalarGrid, (DataType)0); // No tolerance set to clamp values to background value for sparsity.

  return scalarGrid;
}

static openvdb::GridBase::Ptr CopyToGrid(const CopyToGridInput& copyInput, bool convertDoubleToFloat)
{
  openvdb::GridBase::Ptr scalarGrid;
  // Transform the float data to color data
  switch (copyInput.volumeData.DataType)
  {
  case OmniConnectType::CHAR:
    scalarGrid = NormalizedCopyToGridTemplate<char>(copyInput);
    break;
  case OmniConnectType::UCHAR:
    scalarGrid = NormalizedCopyToGridTemplate<unsigned char>(copyInput);
    break;
  case OmniConnectType::SHORT:
    scalarGrid = NormalizedCopyToGridTemplate<short>(copyInput);
    break;
  case OmniConnectType::USHORT:
    scalarGrid = NormalizedCopyToGridTemplate<unsigned short>(copyInput);
    break;
  case OmniConnectType::INT:
    scalarGrid = CopyToGridTemplate<int, openvdb::Int32Grid>(copyInput);
    break;
  case OmniConnectType::UINT:
    scalarGrid = CopyToGridTemplate<unsigned int, openvdb::Int32Grid>(copyInput);
    break;
  case OmniConnectType::LONG:
    scalarGrid = CopyToGridTemplate<long long, openvdb::Int64Grid>(copyInput);
    break;
  case OmniConnectType::ULONG:
    scalarGrid = CopyToGridTemplate<unsigned long long, openvdb::Int64Grid>(copyInput);
    break;
  case OmniConnectType::FLOAT:
    scalarGrid = CopyToGridTemplate<float, openvdb::FloatGrid>(copyInput);
    break;
  case OmniConnectType::DOUBLE:
    if(convertDoubleToFloat)
      scalarGrid = ConvertAndCopyToGridTemplate<DoublePH, openvdb::FloatGrid>(copyInput);
    else
      scalarGrid = CopyToGridTemplate<double, openvdb::DoubleGrid>(copyInput);
    break;
  case OmniConnectType::FLOAT3:
    scalarGrid = CopyToGridTemplate<openvdb::Vec3f, openvdb::Vec3fGrid>(copyInput);
    break;
  case OmniConnectType::DOUBLE3:
    if(convertDoubleToFloat)
      scalarGrid = ConvertAndCopyToGridTemplate<openvdb::Vec3d, openvdb::Vec3fGrid>(copyInput);
    else
      scalarGrid = CopyToGridTemplate<openvdb::Vec3d, openvdb::Vec3dGrid>(copyInput);
    break;
  default:
    {
      const char* typeStr = ocutils::OmniConnectTypeToString(copyInput.volumeData.DataType);
      OmniConnectErrorMacro("Volume writer source data copy does not support source data type: " << typeStr);
      break;
    }
  }
  return scalarGrid;
}

static void CreateGridAndAdd(const OmniConnectVolumeData& volumeData, const openvdb::CoordBBox& bBox, const char* gridName,
  openvdb::math::Transform::Ptr linTrans, bool convertDoubleToFloat, openvdb::GridPtrVecPtr grids)
{
  CopyToGridInput copyToGridInput = { volumeData, bBox };
  openvdb::GridBase::Ptr outGrid = CopyToGrid(copyToGridInput, convertDoubleToFloat);

  if (outGrid)
  {
    outGrid->setName(gridName);

    outGrid->setTransform(linTrans);

    // Push density grid into grid container
    grids->push_back(outGrid);
  }
}

OmniConnectVolumeWriter::OmniConnectVolumeWriter()
  : Internals(std::make_unique<OmniConnectVolumeWriterInternals>())
{
  
}

OmniConnectVolumeWriter::~OmniConnectVolumeWriter()
{
  
}

bool OmniConnectVolumeWriter::Initialize(OmniConnectLogCallback logCallback, void* logUserData)
{
  openvdb::initialize();

  OmniConnectVolumeWriter::LogCallback = logCallback;
  OmniConnectVolumeWriter::LogUserData = logUserData;

  return true;
}

void OmniConnectVolumeWriter::ToVDB(const OmniConnectVolumeData& volumeData,
  OmniConnectGenericArray* updatedGenericArrays, size_t numUga)
{
  const char* densityGridName = "density";
  const char* colorGridName = "diffuse";

  // Keep the grid/tree transform at identity, and set coord bounding box to element dimensions. 
  // This will correspond to a worldspace size of element dimensions too, with rest of scaling handled outside of openvdb.
  const size_t* coordDims = volumeData.NumElements;
  size_t maxInt = std::numeric_limits<int>::max();
  assert(coordDims[0] <= maxInt && coordDims[1] <= maxInt && coordDims[2] <= maxInt);

  // Wrap data in a Dense and copy to color grid
  openvdb::CoordBBox bBox(0, 0, 0, int(coordDims[0] - 1), int(coordDims[1] - 1), int(coordDims[2] - 1)); //Fill is inclusive

  // Compose volume transformation
  openvdb::math::Transform::Ptr linTrans = openvdb::math::Transform::createLinearTransform();
  linTrans->preScale(openvdb::Vec3f(volumeData.CellDimensions));
  linTrans->postTranslate(openvdb::Vec3f(volumeData.Origin));
  
  // Prepare output grids
  openvdb::GridPtrVecPtr grids(new openvdb::GridPtrVec);

  if(volumeData.preClassified)
  {
    OpacityGridOutType::Ptr opacityGrid = OpacityGridOutType::create();
    ColorGridOutType::Ptr colorGrid = ColorGridOutType::create();
    
    opacityGrid->denseFill(bBox, 0.0f, true);
    colorGrid->denseFill(bBox, 
#ifdef FLOAT1_OUTPUT
      0.0f,
#else
      openvdb::Vec3f(0, 0, 0), 
#endif
      true);
    
    // Transform the volumedata and output into color grid
    TfTransformInput tfTransformInput = { colorGrid, opacityGrid, volumeData, bBox };
    SelectTfTransform(tfTransformInput);

    // Set grid names
    opacityGrid->setName(densityGridName);
    colorGrid->setName(colorGridName);

    // Set grid transformation
    opacityGrid->setTransform(linTrans);
    colorGrid->setTransform(linTrans);
    
    // Push color and opacity grid into grid container
    grids->push_back(opacityGrid);
    grids->push_back(colorGrid);
  }
  else
  {
    if(numUga == 0)
    {
      CreateGridAndAdd(volumeData, bBox, densityGridName, linTrans, ConvertDoubleToFloat, grids);
    }
    else
    {
      OmniConnectVolumeData genVolData = volumeData;
      size_t numVolCells = volumeData.NumElements[0]*volumeData.NumElements[1]*volumeData.NumElements[2];

      for(int arrIdx = 0; arrIdx < numUga; ++arrIdx)
      {
        OmniConnectGenericArray& genArr = updatedGenericArrays[arrIdx];

        genVolData.Data = genArr.Data;
        genVolData.DataType = genArr.DataType;

        if(numVolCells == genArr.NumElements)
        {
          CreateGridAndAdd(genVolData, bBox, genArr.Name, linTrans, ConvertDoubleToFloat, grids);
        }
      }
    }
  }

  // Must write all grids at once
  openvdb::io::Stream(Internals->ResetStream()).write(*grids);
}

void OmniConnectVolumeWriter::GetSerializedVolumeData(const char*& data, size_t& size)
{
  data = Internals->GetStreamData();
  size = Internals->GetStreamDataSize();
}

#else //USE_OPENVDB

OmniConnectVolumeWriter::OmniConnectVolumeWriter()
{

}

OmniConnectVolumeWriter::~OmniConnectVolumeWriter()
{

}

bool OmniConnectVolumeWriter::Initialize(OmniConnectLogCallback logCallback, void* logUserData)
{
  return true;
}

void OmniConnectVolumeWriter::ToVDB(const OmniConnectVolumeData & volumeData,
  OmniConnectGenericArray* updatedGenericArrays, size_t numUga)
{

}

void OmniConnectVolumeWriter::GetSerializedVolumeData(const char*& data, size_t& size)
{
  data = nullptr;
  size = 0;
}

#endif //USE_OPENVDB

