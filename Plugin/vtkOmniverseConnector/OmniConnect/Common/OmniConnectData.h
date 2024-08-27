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

#ifndef OmniConnectData_h
#define OmniConnectData_h

#define USE_MDL_MATERIALS
#define USE_INDEX_MATERIALS

#define USE_CUSTOM_MDL 0
#define USE_CUSTOM_POINT_SHADER 0

#define MAX_USER_STRLEN 16384

#include <functional>
#include <stdint.h>

enum class OmniConnectType
{
  UCHAR = 0,
  CHAR,
  USHORT,
  SHORT,
  UINT,
  INT,
  ULONG,
  LONG,
  FLOAT,
  DOUBLE,

  UCHAR2,
  CHAR2,
  USHORT2,
  SHORT2,
  UINT2,
  INT2,
  ULONG2,
  LONG2,
  FLOAT2,
  DOUBLE2,

  UCHAR3,
  CHAR3,
  USHORT3,
  SHORT3,
  UINT3,
  INT3,
  ULONG3,
  LONG3,
  FLOAT3,
  DOUBLE3,

  UCHAR4,
  CHAR4,
  USHORT4,
  SHORT4,
  UINT4,
  INT4,
  ULONG4,
  LONG4,
  FLOAT4,
  DOUBLE4,

  UNDEFINED
};
constexpr int OmniConnectNumFundamentalTypes = (int)OmniConnectType::UCHAR2; // Multi-component groups sizes should be equal

enum class OmniConnectStatusType
{
  UPDATE,
  REMOVE,
  KEEP
};

enum class OmniConnectGeomType
{
  MESH = 0,
  INSTANCER,
  CURVE,
  VOLUME,
  NUM_GEOMTYPES
};

enum class OmniConnectVolumeFieldType
{
  DENSITY,
  COLOR
};

enum class OmniConnectLogLevel
{
  STATUS,
  WARNING,
  ERR
};

enum class OmniConnectAxis
{
  X,
  Y,
  Z
};

typedef void(*OmniConnectLogCallback)(OmniConnectLogLevel, void*, const char*);

void OmniConnectAuthCallbackFunc(void* userData, bool show, const char* server, uint32_t authHandle) noexcept;
typedef decltype(OmniConnectAuthCallbackFunc) OmniConnectAuthCallback; // Guard against compilers which don't accept `noexcept`

struct OmniConnectSettings
{
  const char* OmniServer;           // Omniverse server to connect to
  const char* OmniWorkingDirectory; // Base directory in which the PV session directories are created, containing scene + actor usd files and assets
  const char* LocalOutputDirectory; // Directory for local output (used in case OutputLocal is enabled)
  const char* RootLevelFileName;    // When set, a root level USD file will be created of the specified name, that sublayers highest level layer, with the session folder renamed to the same name.
  bool OutputLocal;                 // Output to OmniLocalDirectory instead of the Omniverse
  bool OutputBinary;                // Output binary usd files instead of text-based usda
  OmniConnectAxis UpAxis;           // Up axis of USD output
  bool UsePointInstancer;           // Either use UsdGeomPointInstancer for point data, or otherwise UsdGeomPoints
  bool UseMeshVolume;               // Represent volumes as regular textured meshes
  bool CreateNewOmniSession;        // Find a new Omniverse session directory on creation of the connector, or re-use the last opened one.
};

struct OmniConnectEnvironment
{
  int ProcId;                       // ProcId, >= 0 in case the connector instance is running in a multiprocessor environment
  int NumProcs;                     // Num of procs
};

struct OmniConnectGenericArray
{
  //Constructors
  OmniConnectGenericArray() = default;
  ~OmniConnectGenericArray() = default;
  OmniConnectGenericArray(const char* name, bool perPoly, bool timeVarying, void* data, int numElements, OmniConnectType dataType)
    : Name(name), PerPoly(perPoly), TimeVarying(timeVarying), Data(data), NumElements(numElements), DataType(dataType) {}
  OmniConnectGenericArray(OmniConnectGenericArray& other) = default;
  OmniConnectGenericArray(OmniConnectGenericArray&& other) = default;
  OmniConnectGenericArray& operator=(OmniConnectGenericArray&& rhs) = default;

  //Data
  const char* Name;
  bool PerPoly;
  bool TimeVarying;

  void* Data;
  size_t NumElements;
  OmniConnectType DataType;
};

struct OmniConnectMeshData
{
  static const OmniConnectGeomType GeomType = OmniConnectGeomType::MESH;

  //Clas definitions
  enum class DataMemberId : uint32_t
  {
    NONE = 0,
    POINTS = (1 << 0), // Goes together with extent
    NORMALS = (1 << 1),
    TEXCOORDS = (1 << 2),
    COLORS = (1 << 3),
    INDICES = (1 << 4),
    ALL = (1 << 5) - 1
  };
  
  DataMemberId UpdatesToPerform = DataMemberId::ALL;
  DataMemberId TimeVarying = DataMemberId::ALL;

  bool isEmpty() { return Points == NULL; }

  //Mesh data
  size_t MeshId = 0; // Id unique to meshes only (sometimes generalized to geomId, but those are not unique)

  uint64_t NumPoints = 0;
	
  void* Points = nullptr; 
  OmniConnectType PointsType = OmniConnectType::UNDEFINED;
  void* Normals = nullptr;
  OmniConnectType NormalsType = OmniConnectType::UNDEFINED;
  bool PerPrimNormals = false;
  void* TexCoords = nullptr;
  OmniConnectType TexCoordsType = OmniConnectType::UNDEFINED;
  bool PerPrimTexCoords = false;
  unsigned char* Colors = nullptr;
  bool PerPrimColors = false;
  int ColorComponents = 0;

  unsigned int* Indices = nullptr;
  uint64_t NumIndices = 0;
};

struct OmniConnectInstancerData
{
  static const OmniConnectGeomType GeomType = OmniConnectGeomType::INSTANCER;

  //Class definitions
  enum InstanceShape
  {
    SHAPE_SPHERE = -1,
    SHAPE_CYLINDER = -2,
    SHAPE_CONE = -3,
    SHAPE_CUBE = -4,
    SHAPE_ARROW = -5,
    SHAPE_MESH = 0 // Positive values denote meshes (specific meshes can be referenced by id)
  };

  enum class DataMemberId : uint32_t
  {
    NONE = 0,
    POINTS = (1 << 0), // Goes together with extent
    SHAPES = (1 << 1), // Cannot be timevarying
    SHAPEINDICES = (1 << 2),
    SCALES = (1 << 3),
    ORIENTATIONS = (1 << 4),
    LINEARVELOCITIES = (1 << 5),
    ANGULARVELOCITIES = (1 << 6),
    INSTANCEIDS = (1 << 7),
    TEXCOORDS = (1 << 8),
    COLORS = (1 << 9),
    INVISIBLEINDICES = (1 << 10),
    ALL = (1 << 11) - 1
  };

  DataMemberId UpdatesToPerform = DataMemberId::ALL;
  DataMemberId TimeVarying = (DataMemberId)(~((uint32_t)DataMemberId::SHAPEINDICES));

  //Instancer data
  size_t InstancerId = 0; // Id unique to instancers only (sometimes generalized to geomId, but those are not unique)

  InstanceShape DefaultShape = SHAPE_SPHERE;
  InstanceShape* Shapes = &DefaultShape; //no duplicate entries allowed.
  int NumShapes = 1;
  const char* ShapeSourceNameSubstr = nullptr;
  float ShapeDims[3] = {0.5f, 0.5f, 0.5f}; // Converted to protoshape transform ops (sphere: (radius,-,-) cylinder: (radius, length/2, -), cone: (radius, length/2, -), cube: (x/2,y/2,z/2), arrow(conepercent, cylradius, coneradius), mesh: (x/2,y/2,z/2))

  uint64_t NumPoints = 0;
  void* Points = nullptr; 
  OmniConnectType PointsType = OmniConnectType::UNDEFINED;
  int* ShapeIndices = nullptr; //if set, one for every point
  void* Scales = nullptr;// 3-vector scale
  OmniConnectType ScalesType = OmniConnectType::UNDEFINED;
  double UniformScale = 1;// In case no scales are given
  void* Orientations = nullptr;
  OmniConnectType OrientationsType = OmniConnectType::UNDEFINED;
  void* TexCoords = nullptr;
  OmniConnectType TexCoordsType = OmniConnectType::UNDEFINED;
  static constexpr bool  PerPrimTexCoords = false;
  unsigned char* Colors = nullptr;
  int ColorComponents = 0;
  static constexpr bool PerPrimColors = false; // For compatibility
  float* LinearVelocities = nullptr; 
  float* AngularVelocities = nullptr; 
  int* InstanceIds = nullptr;

  int64_t* InvisibleIndices = nullptr; //Index into points
  uint64_t NumInvisibleIndices = 0;
};

struct OmniConnectCurveData
{
  static const OmniConnectGeomType GeomType = OmniConnectGeomType::CURVE;

  //Class definitions
  enum class DataMemberId : uint32_t
  {
    NONE = 0,
    POINTS = (1 << 0), // Goes together with extent and curvelengths
    NORMALS = (1 << 1),
    SCALES = (1 << 2),
    TEXCOORDS = (1 << 3),
    COLORS = (1 << 4),
    CURVELENGTHS = (1 << 5),
    ALL = (1 << 6) - 1
  };

  DataMemberId UpdatesToPerform = DataMemberId::ALL;
  DataMemberId TimeVarying = DataMemberId::ALL;

  bool isEmpty() { return Points == NULL; }

  //Curve data
  size_t CurveId = 0; // Id unique to curves only (sometimes generalized to geomId, but those are not unique)

  uint64_t NumPoints = 0;

  void* Points = nullptr;
  OmniConnectType PointsType = OmniConnectType::UNDEFINED;
  void* Normals = nullptr;
  OmniConnectType NormalsType = OmniConnectType::UNDEFINED;
  bool PerPrimNormals = false;
  void* TexCoords = nullptr;
  OmniConnectType TexCoordsType = OmniConnectType::UNDEFINED;
  bool PerPrimTexCoords = false;
  unsigned char* Colors = nullptr;
  bool PerPrimColors = false; // One prim would be a full curve
  int ColorComponents = 0;
  void* Scales = nullptr; // Used for line width, typically 1-component
  OmniConnectType ScalesType = OmniConnectType::UNDEFINED;
  double UniformScale = 1;// In case no scales are given

  int* CurveLengths = nullptr;
  uint64_t NumCurveLengths = 0;
};

struct OmniConnectTfData
{
  const void* TfColors = nullptr;
  OmniConnectType TfColorsType = OmniConnectType::UNDEFINED;
  const void* TfOpacities = nullptr;
  OmniConnectType TfOpacitiesType = OmniConnectType::UNDEFINED;
  int TfNumValues = 0;
  double TfValueRange[2] = { 0, 1 };
};

struct OmniConnectVolumeData
{
  static const OmniConnectGeomType GeomType = OmniConnectGeomType::VOLUME;

  enum class DataMemberId : uint32_t
  {
    NONE = 0,
    DATA = (1 << 1),
    ALL = (1 << 2) - 1
  };

  DataMemberId UpdatesToPerform = DataMemberId::ALL;
  DataMemberId TimeVarying = DataMemberId::ALL;

  //Volume data
  size_t VolumeId = 0; // Id unique to volumes only (sometimes generalized to geomId, but those are not unique)

  bool preClassified = false;

  const void* Data = nullptr;
  OmniConnectType DataType = OmniConnectType::UNDEFINED; // Same timeVarying rule as 'Data'
  size_t NumElements[3] = { 0,0,0 }; // Same timeVarying rule as 'Data'
  float Origin[3] = { 0,0,0 };
  float CellDimensions[3] = { 1,1,1 };

  long long BackgroundIdx; // When not -1, denotes the first element in Data which contains a background value

  OmniConnectTfData TfData;
};


// Material data
struct OmniConnectMaterialData
{
  size_t MaterialId = 0;

  bool VolumeMaterial = false;

  enum class DataMemberId : uint32_t
  {
    NONE = 0,
    DIFFUSE = (1 << 0),
    SPECULAR = (1 << 1),
    EMISSIVE = (1 << 2),
    OPACITY = (1 << 3),
    EMISSIVEINTENSITY = (1 << 4),
    ROUGHNESS = (1 << 5),
    METALLIC = (1 << 6),
    IOR = (1 << 7),
    TF = (1 << 8),
    ALL = (1 << 9) - 1
  };
  DataMemberId TimeVarying = DataMemberId::NONE;

  float Diffuse[3] = { 1.0f };
  float Specular[3] = { 1.0f };
  float Emissive[3] = { 1.0f };
  float Opacity = 0;
  bool OpacityMapped = false;
  bool HasTranslucency = false;
  float EmissiveIntensity = 0;
  float Roughness = 0;
  float Metallic = 0.0;
  float Ior = 1.0f;
  bool UseVertexColors = false;
#if defined(USE_MDL_MATERIALS) && USE_CUSTOM_POINT_SHADER
  bool UsePointShader = false;
#endif
  
  size_t TexId = 0;
  void* EmissiveTextureData = nullptr;

  // Volume data
  OmniConnectType VolumeDataType;
  OmniConnectTfData TfData;
};

struct OmniConnectSamplerData
{
  enum class DataMemberId : uint32_t
  {
    NONE = 0,
    FILENAME = (1 << 0),
    WRAPS = (1 << 1),
    WRAPT = (1 << 2),
    ALL = (1 << 3) - 1
  };
  DataMemberId TimeVarying = DataMemberId::NONE;

  enum class WrapMode
  {
    BLACK = 0,
    CLAMP,
    REPEAT,
    MIRROR
  };

  const char* TexData = nullptr;
  size_t TexDataSize = 0;
  WrapMode WrapS = WrapMode::BLACK;
  WrapMode WrapT = WrapMode::BLACK;
};

struct OmniConnectUrlInfo
{
  const char* Url = nullptr;
  const char* Etag = nullptr;
  time_t ModifiedTime;
  uint64_t Size = 0;
  const char* Author = nullptr;
  bool IsFile = false;
  bool operator<(const OmniConnectUrlInfo& v) const { return Url < v.Url; }
  bool operator==(const OmniConnectUrlInfo& v) const { return Url == v.Url; }
  bool operator!=(const OmniConnectUrlInfo& v) const { return Url != v.Url; }
};

struct OmniConnectUrlInfoList
{
  OmniConnectUrlInfo* infos = nullptr;
  size_t size = 0;
};

class OmniConnectImageWriter
{
public:
  virtual bool WriteData(void* omniImage) = 0;
  virtual void GetResult(char*& imageData, int& imageDataSize) = 0;
};

template<class TEnum>
struct DefineBitMaskOps
{
  static const bool enable = false;
};

template<>
struct DefineBitMaskOps<OmniConnectMeshData::DataMemberId>
{
  static const bool enable = true; 
};

template<>
struct DefineBitMaskOps<OmniConnectInstancerData::DataMemberId>
{
  static const bool enable = true;
};

template<>
struct DefineBitMaskOps<OmniConnectCurveData::DataMemberId>
{
  static const bool enable = true;
};

template<>
struct DefineBitMaskOps<OmniConnectVolumeData::DataMemberId>
{
  static const bool enable = true;
};

template<>
struct DefineBitMaskOps<OmniConnectMaterialData::DataMemberId>
{
  static const bool enable = true;
};

template<class TEnum>
typename std::enable_if<DefineBitMaskOps<TEnum>::enable, TEnum>::type operator |(TEnum lhs, TEnum rhs)
{
  using underlying = typename std::underlying_type<TEnum>::type;
  return static_cast<TEnum> (
    static_cast<underlying>(lhs) |
    static_cast<underlying>(rhs)
    );
}

template<class TEnum>
typename std::enable_if<DefineBitMaskOps<TEnum>::enable, TEnum>::type operator &(TEnum lhs, TEnum rhs)
{
  using underlying = typename std::underlying_type<TEnum>::type;
  return static_cast<TEnum> (
    static_cast<underlying>(lhs) &
    static_cast<underlying>(rhs)
    );
}

template<class TEnum>
typename std::enable_if<DefineBitMaskOps<TEnum>::enable, TEnum>::type operator ~(TEnum rhs)
{
  using underlying = typename std::underlying_type<TEnum>::type;
  return static_cast<TEnum> (~static_cast<underlying>(rhs));
}

template<class T>
class OmniConnectUpdateEvaluator
{
public:
  OmniConnectUpdateEvaluator(T& data)
    : Data(data)
  {
  }

  bool PerformsUpdate(typename T::DataMemberId member) const
  {
    return ((Data.UpdatesToPerform & member) != T::DataMemberId::NONE);
  }

  void RemoveUpdate(typename T::DataMemberId member)
  { 
    Data.UpdatesToPerform = (Data.UpdatesToPerform & ~member);
  }

  void AddUpdate(typename T::DataMemberId member) 
  { 
    Data.UpdatesToPerform = (Data.UpdatesToPerform | member);
  }

  T& Data;
};

#endif