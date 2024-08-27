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

#include "OmniConnect.h"

#include "usd.h"
PXR_NAMESPACE_USING_DIRECTIVE

#include <assert.h>
#include <string>

#include <iostream>
#include <fstream>
#include <functional>
#include <memory>

#include "OmniConnectConnection.h"
#include "OmniConnectCaches.h"
#include "OmniConnectMdl.h"
#include "OmniConnectVolumeWriter.h"
#include "OmniConnectUsdUtils.h"
#include "OmniConnectLiveInterface.h"
#include "OmniConnectDiagnosticMgrDelegate.h"
#include "OmniConnectUtilsInternal.h"

//#define FORCE_OMNI_CLIP_UPDATES_WITH_DUMMY
//#define FORCE_OMNI_CLIP_UPDATES_WITH_SUBLAYERS
//#define TOPOLOGY_AS_SUBLAYER

#define OmniConnectErrorMacro(x) \
  { std::stringstream logStream; \
    logStream << x; \
    std::string logString = logStream.str(); \
    OmniConnectInternals::LogCallback( OmniConnectLogLevel::ERR, nullptr, logString.c_str()); } 

#define OmniConnectDebugMacro(x) \
  { std::stringstream logStream; \
    logStream << x; \
    std::string logString = logStream.str(); \
    OmniConnectInternals::LogCallback( OmniConnectLogLevel::WARNING, nullptr, logString.c_str()); }  

const char* OmniMaterialRelativePath = "materials/";
const char* OmniGeomRelativePath = "geometries/";
const char* OmniTopologyRelativePath = "topologies/";
const char* OmniTextureRelativePath = "textures/";
const char* OmniVolumeRelativePath = "volumes/";
#ifdef FORCE_OMNI_CLIP_UPDATES_WITH_DUMMY
const char* OmniDummyLayerPath = "dummylayers/";
#endif

static std::ofstream myfile;

// Map OmniConnectGeomCaches to their Usd Prim type
template<typename CacheType>
struct UsdTypeFromCache
{};
template<>
struct UsdTypeFromCache<OmniConnectMeshCache>
{
  typedef UsdGeomMesh UsdPrimType;
  typedef UsdGeomMesh AltUsdPrimType;
};
template<>
struct UsdTypeFromCache<OmniConnectInstancerCache>
{
  typedef UsdGeomPointInstancer UsdPrimType;
  typedef UsdGeomPoints AltUsdPrimType;
};
template<>
struct UsdTypeFromCache<OmniConnectCurveCache>
{
  typedef UsdGeomBasisCurves UsdPrimType;
  typedef UsdGeomBasisCurves AltUsdPrimType;
};
template<>
struct UsdTypeFromCache<OmniConnectVolumeCache>
{
  typedef UsdVolVolume UsdPrimType;
  typedef UsdGeomMesh AltUsdPrimType;
};

template<typename UsdGeomType>
UsdAttribute UsdGeomGetPointsAttribute(UsdGeomType& usdGeom) { return UsdAttribute(); }

template<>
UsdAttribute UsdGeomGetPointsAttribute<UsdGeomMesh>(UsdGeomMesh& usdGeom) { return usdGeom.GetPointsAttr(); }

template<>
UsdAttribute UsdGeomGetPointsAttribute<UsdGeomPoints>(UsdGeomPoints& usdGeom) { return usdGeom.GetPointsAttr(); }

template<>
UsdAttribute UsdGeomGetPointsAttribute<UsdGeomBasisCurves>(UsdGeomBasisCurves& usdGeom) { return usdGeom.GetPointsAttr(); }

template<>
UsdAttribute UsdGeomGetPointsAttribute<UsdGeomPointInstancer>(UsdGeomPointInstancer& usdGeom) { return usdGeom.GetPositionsAttr(); }

template<typename UsdGeomType>
void UsdGeomCreatePointsAttribute(UsdGeomType& usdGeom) { }

template<>
void UsdGeomCreatePointsAttribute<UsdGeomMesh>(UsdGeomMesh& usdGeom) { usdGeom.CreatePointsAttr(); }

template<>
void UsdGeomCreatePointsAttribute<UsdGeomPoints>(UsdGeomPoints& usdGeom) { usdGeom.CreatePointsAttr(); }

template<>
void UsdGeomCreatePointsAttribute<UsdGeomBasisCurves>(UsdGeomBasisCurves& usdGeom) { usdGeom.CreatePointsAttr(); }

template<>
void UsdGeomCreatePointsAttribute<UsdGeomPointInstancer>(UsdGeomPointInstancer& usdGeom) { usdGeom.CreatePositionsAttr(); }

template<typename UsdGeomType>
void UsdGeomRemovePointsAttribute(UsdGeomType& usdGeom) { }

template<>
void UsdGeomRemovePointsAttribute<UsdGeomMesh>(UsdGeomMesh& usdGeom) { usdGeom.GetPrim().RemoveProperty(OmniConnectTokens->points); }

template<>
void UsdGeomRemovePointsAttribute<UsdGeomPoints>(UsdGeomPoints& usdGeom) { usdGeom.GetPrim().RemoveProperty(OmniConnectTokens->points); }

template<>
void UsdGeomRemovePointsAttribute<UsdGeomBasisCurves>(UsdGeomBasisCurves& usdGeom) { usdGeom.GetPrim().RemoveProperty(OmniConnectTokens->points); }

template<>
void UsdGeomRemovePointsAttribute<UsdGeomPointInstancer>(UsdGeomPointInstancer& usdGeom) { usdGeom.GetPrim().RemoveProperty(OmniConnectTokens->positions); }

namespace
{
  TfToken GetTokenFromFieldType(OmniConnectVolumeFieldType fieldType)
  {
    TfToken token = OmniConnectTokens->density;
    switch (fieldType)
    {
    case OmniConnectVolumeFieldType::DENSITY:
      token = OmniConnectTokens->density;
      break;
    case OmniConnectVolumeFieldType::COLOR:
      token = OmniConnectTokens->diffuse;
      break;
    default:
      break;
    };
    return token;
  }

  void UsdTrace(const char* line)
  {
    //myfile << line << "\n";
  }

  void FormatDirName(std::string& dirName)
  {
    if (dirName.length() > 0)
    {
      if (dirName.back() != '/' && dirName.back() != '\\')
        dirName.append("/");
    }
  }

  template<typename PrimType>
  PrimType DefineOnEmptyPrim(UsdStageRefPtr& stage, const SdfPath& path)
  {
    if(stage->GetPrimAtPath(path))
        stage->RemovePrim(path); // This can happen in very specific instances where a primtype is changed at runtime (generally not supported, but admitted for pointinstancer <-> points in case of glyphs)
    return PrimType::Define(stage, path); 
  }

  UsdGeomPrimvar GetPrimvarForType(UsdGeomPrimvarsAPI& primvarAPI, const TfToken& nameToken, const SdfValueTypeName& valueTypeName)
  {
    UsdGeomPrimvar primvar = primvarAPI.GetPrimvar(nameToken);
    if(primvar && primvar.GetTypeName() != valueTypeName)
    {
        primvarAPI.RemovePrimvar(nameToken);
        return UsdGeomPrimvar();
    }
    return primvar;
  }

  UsdAttribute GetPrimvarByName(UsdGeomPrimvarsAPI& actorPrimvarsApi, UsdGeomPrimvarsAPI& clipPrimvarsApi, UsdGeomPrimvarsAPI& topPrimvarsApi, const char* primvarName, const TimeEvaluator<bool>& timeEval, const SdfValueTypeName& valueTypeName, bool perPoly)
  {
    TfToken nameToken(primvarName);

    UsdGeomPrimvar actPrimvar = GetPrimvarForType(actorPrimvarsApi, nameToken, valueTypeName);
    UsdGeomPrimvar clipPrimvar = GetPrimvarForType(clipPrimvarsApi, nameToken, valueTypeName);
    UsdGeomPrimvar topPrimvar = GetPrimvarForType(topPrimvarsApi, nameToken, valueTypeName);

    // Create the primvar if it doesn't exist, otherwise clear at time
    UsdGeomPrimvarsAPI& primvarAPIRef = timeEval.TimeVarying ? clipPrimvarsApi : actorPrimvarsApi;
    UsdGeomPrimvar& primvarRef = timeEval.TimeVarying ? clipPrimvar : actPrimvar;
    if (primvarRef)
    {
      primvarRef.GetAttr().ClearAtTime(timeEval.Eval());
    }
    else
    {
      primvarRef = primvarAPIRef.CreatePrimvar(nameToken,
        valueTypeName);
    }

    // Update the primvar in the manifest
    if (timeEval.TimeVarying && !topPrimvar)
    {
      topPrimvarsApi.CreatePrimvar(nameToken,
        valueTypeName);
    }
    else if(!timeEval.TimeVarying && topPrimvar)
    {
      topPrimvarsApi.RemovePrimvar(nameToken);
    }

    // Set interpolation 
    if(!actPrimvar)
      actPrimvar = actorPrimvarsApi.CreatePrimvar(nameToken, valueTypeName);

    TfToken ipolToken = perPoly ? UsdGeomTokens->uniform : UsdGeomTokens->vertex;
    actPrimvar.SetInterpolation(ipolToken);

    // Not strictly necessary for USD, but cleaner output
    if (timeEval.TimeVarying) // Create requires this branch
    { 
      actPrimvar.GetAttr().Clear(); 
    }
    else if (clipPrimvar)
    {
      clipPrimvar.GetAttr().ClearAtTime(timeEval.TimeCode); // clear at clip timecode
    }

    return primvarRef;
  }

  template<class ArrayType>
  void AssignArrayToPrimvar(void* data, size_t numElements, UsdAttribute& primvar, const UsdTimeCode& timeCode, ArrayType* usdArray)
  {
    using ElementType = typename ArrayType::ElementType;
    ElementType* typedData = (ElementType*)data;
    usdArray->assign(typedData, typedData + numElements);
  }

  template<class ArrayType>
  void AssignArrayToPrimvarFlatten(void* data, OmniConnectType dataType, size_t numElements, UsdAttribute& primvar, const UsdTimeCode& timeCode, ArrayType* usdArray)
  {
    int elementMultiplier = (int)dataType / OmniConnectNumFundamentalTypes;
    size_t numFlattenedElements = numElements * elementMultiplier;

    AssignArrayToPrimvar<ArrayType>(data, numFlattenedElements, primvar, timeCode, usdArray);
  }

  template<class ArrayType, class EltType>
  void AssignArrayToPrimvarConvert(void* data, size_t numElements, UsdAttribute& primvar, const UsdTimeCode& timeCode, ArrayType* usdArray)
  {
    using ElementType = typename ArrayType::ElementType;
    EltType* typedData = (EltType*)data;

    usdArray->resize(numElements);
    for (int i = 0; i < numElements; ++i)
    {
      (*usdArray)[i] = ElementType(typedData[i]);
    }
  }

  template<class ArrayType, class EltType>
  void AssignArrayToPrimvarConvertFlatten(void* data, OmniConnectType dataType, size_t numElements, UsdAttribute& primvar, const UsdTimeCode& timeCode, ArrayType* usdArray)
  {
    int elementMultiplier = (int)dataType / OmniConnectNumFundamentalTypes;
    size_t numFlattenedElements = numElements * elementMultiplier;

    AssignArrayToPrimvarConvert<ArrayType, EltType>(data, numFlattenedElements, primvar, timeCode, usdArray);
  }

  template<class ArrayType, class EltType>
  void AssignArrayToPrimvarFirstComponent(const void* data, size_t numElements, UsdAttribute& primvar, const UsdTimeCode& timeCode, ArrayType* usdArray)
  {
    using ElementType = typename ArrayType::ElementType;
    EltType* typedData = (EltType*)data;

    usdArray->resize(numElements);
    for (int i = 0; i < numElements; ++i)
    {
      (*usdArray)[i] = ElementType(typedData[i][0]);
    }
  }

  template<typename NormalsType>
  void ConvertNormalsToQuaternions(VtQuathArray& quaternions, const void* normals, uint64_t numVertices)
  {
    GfVec3f from(0.0f, 0.0f, 1.0f);
    NormalsType* norms = (NormalsType*)(normals);
    for (int i = 0; i < numVertices; ++i)
    {
      GfVec3f to((float)(norms[i * 3]), (float)(norms[i * 3 + 1]), (float)(norms[i * 3 + 2]));
      GfRotation rot(from, to);
      const GfQuaternion& quat = rot.GetQuaternion();
      quaternions[i] = GfQuath((float)(quat.GetReal()), GfVec3h(quat.GetImaginary()));
    }
  }

  template<typename T>
  float GetShapeWidth(const T& omniGeomData) { return 1.0f; }

  template<>
  float GetShapeWidth<OmniConnectInstancerData>(const OmniConnectInstancerData& omniGeomData) { return omniGeomData.ShapeDims[0]*2.0f; }
}

#define GET_PRIMVAR_BY_NAME_MACRO(valueTypeName) \
  UsdAttribute arrayPrimvar = GetPrimvarByName(actorPrimvarsApi, clipPrimvarsApi, topPrimvarsApi, primvarName, timeEval, valueTypeName, perPoly);

#define ASSIGN_SET_PRIMVAR if(setPrimvar) arrayPrimvar.Set(usdArray, timeCode)
#define ASSIGN_ARRAY_TO_PRIMVAR_MACRO(ArrayType) \
  ArrayType usdArray; AssignArrayToPrimvar<ArrayType>(arrayData, arrayNumElements, arrayPrimvar, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_ARRAY_TO_PRIMVAR_FLATTEN_MACRO(ArrayType) \
  ArrayType usdArray; AssignArrayToPrimvarFlatten<ArrayType>(arrayData, dataType, arrayNumElements, arrayPrimvar, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(ArrayType, EltType) \
  ArrayType usdArray; AssignArrayToPrimvarConvert<ArrayType, EltType>(arrayData, arrayNumElements, arrayPrimvar, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_FLATTEN_MACRO(ArrayType, EltType) \
  ArrayType usdArray; AssignArrayToPrimvarConvertFlatten<ArrayType, EltType>(arrayData, dataType, arrayNumElements, arrayPrimvar, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_ARRAY_TO_PRIMVAR_FIRST_COMPONENT_MACRO(ArrayType, EltType) \
  ArrayType usdArray; AssignArrayToPrimvarFirstComponent<ArrayType, EltType>(arrayData, arrayNumElements, arrayPrimvar, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_CUSTOM_ARRAY_TO_PRIMVAR_MACRO(ArrayType, customArray) \
  ArrayType& usdArray = customArray; AssignArrayToPrimvar<ArrayType>(arrayData, arrayNumElements, arrayPrimvar, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_CUSTOM_ARRAY_TO_PRIMVAR_FLATTEN_MACRO(ArrayType, customArray) \
  ArrayType& usdArray = customArray; AssignArrayToPrimvarFlatten<ArrayType>(arrayData, dataType, arrayNumElements, arrayPrimvar, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_CUSTOM_ARRAY_TO_PRIMVAR_CONVERT_MACRO(ArrayType, EltType, customArray) \
  ArrayType& usdArray = customArray; AssignArrayToPrimvarConvert<ArrayType, EltType>(arrayData, arrayNumElements, arrayPrimvar, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_CUSTOM_ARRAY_TO_PRIMVAR_CONVERT_FLATTEN_MACRO(ArrayType, EltType, customArray) \
  ArrayType& usdArray = customArray; AssignArrayToPrimvarConvertFlatten<ArrayType, EltType>(arrayData, dataType, arrayNumElements, arrayPrimvar, timeCode, &usdArray); ASSIGN_SET_PRIMVAR
#define ASSIGN_CUSTOM_ARRAY_TO_PRIMVAR_FIRST_COMPONENT_MACRO(ArrayType, EltType, customArray) \
  ArrayType& usdArray = customArray; AssignArrayToPrimvarFirstComponent<ArrayType, EltType>(arrayData, arrayNumElements, arrayPrimvar, timeCode, &usdArray); ASSIGN_SET_PRIMVAR

//Includes detailed usd translation interface of OmniConnect
class OmniConnectInternals
{
public:

  OmniConnectInternals(const OmniConnectSettings& settings
    , const OmniConnectEnvironment& environment
    , OmniConnectConnection* connection
    , OmniConnectLogCallback logCallback)
    : Connection(connection)
    , VolumeWriter(Create_VolumeWriter(), std::mem_fn(&OmniConnectVolumeWriterI::Release))
    , Settings(settings)
    , Environment(environment)
  {
    OmniConnectInternals::LogCallback = logCallback;

    VolumeWriter->Initialize(OmniConnectInternals::LogCallback, nullptr);
    //myfile.open("d:\\KVK\\Debug.txt");

    DiagnosticDelegate = std::make_unique<OmniConnectDiagnosticMgrDelegate>(nullptr, logCallback);
    DiagRemoveFunc = [](OmniConnectDiagnosticMgrDelegate* delegate)
      { TfDiagnosticMgr::GetInstance().RemoveDelegate(delegate); };
    TfDiagnosticMgr::GetInstance().AddDelegate(DiagnosticDelegate.get());
  }

  ~OmniConnectInternals()
  {
    if(DiagRemoveFunc)
      DiagRemoveFunc(DiagnosticDelegate.get());

    //myfile.close();
  }

  int FindSessionNumber();
  void InitializeSession();

  void OpenRootLevelStage();
  void OpenMultiSceneStage();
  void OpenSceneStage();
  void CreateDefaultLighting(UsdStageRefPtr& stage);
  void OpenActorStage(OmniConnectActorCache& actorCache);
  void OpenClipAndTopologyStage(const char* clipFilePath, const char* topologyFilePath, UsdStageRefPtr& clipStage, UsdStageRefPtr& topologyStage);
  UsdShadeOutput CreateUsdPreviewSurface(OmniConnectActorCache& actorCache, OmniConnectMatCache& matCache, UsdShadeShader& shader, bool newMat);
  void ResetUsdPreviewSurface(UsdShadeShader& shader);
  void UpdateUsdPreviewSurface(OmniConnectActorCache& actorCache, OmniConnectMatCache& matCache, const OmniConnectTexCache* texCache, const OmniConnectMaterialData& omniMatData, UsdShadeShader& shader, double animTimeStep);
  void UpdateActorMaterial(OmniConnectActorCache& actorCache, const OmniConnectMaterialData& omniMatData, double animTimeStep);
  void RemoveActorMaterial(OmniConnectActorCache& actorCache, size_t matId);
  UsdShadeShader InitializeUsdTexture(OmniConnectActorCache& actorCache, OmniConnectSamplerData& samplerData, OmniConnectTexCache& texCache);
  void UpdateUsdTexture(OmniConnectTexCache& texCache, UsdShadeShader& textureReader, bool timeVarying, double animTimeStep);
  void UpdateTexture(OmniConnectActorCache& actorCache, size_t texId, OmniConnectSamplerData& samplerData, bool timeVarying, double animTimeStep);
  void DeleteTexture(OmniConnectActorCache& actorCache, size_t texCacheIdx);
  void UpdateGenericArrays(UsdGeomPrimvarsAPI& actorPrimvarsApi, UsdGeomPrimvarsAPI& clipPrimvarsApi, UsdGeomPrimvarsAPI& topPrimvarsApi, OmniConnectGenericArray* genericArrays, size_t numGenericArrays, double animTimeStep);
  void DeleteGenericArrays(UsdGeomPrimvarsAPI& actorPrimvarsApi, UsdGeomPrimvarsAPI& clipPrimvarsApi, UsdGeomPrimvarsAPI& topPrimvarsApi, OmniConnectGenericArray* genericArrays, size_t numGenericArrays, double animTimeStep);
  template<typename CacheType> void UpdateGeom(OmniConnectActorCache* actorCache, double animTimeStep, typename CacheType::GeomDataType& geomData, size_t geomId, size_t materialId,
    OmniConnectGenericArray* updatedGenericArrays, size_t numUga, OmniConnectGenericArray* deletedGenericArrays, size_t numDga);
  template<typename CacheType> bool RemoveGeomAtTime(OmniConnectActorCache* actorCache, double animTimeStep, size_t geomId);
  template<typename CacheType> void RemoveGeom(OmniConnectActorCache* actorCache, size_t geomId);
  template<typename CacheType> void CreateSceneGeom(UsdStageRefPtr& sceneStage, const OmniConnectGeomCache& geomCache);
  template<typename CacheType> void UpdateSceneGeom(UsdStageRefPtr& sceneStage, const CacheType& geomCache, typename CacheType::GeomDataType& geomData);
  void RemoveSceneGeom(UsdStageRefPtr& sceneStage, const SdfPath& geomPath);
  template<typename CacheType, typename PrimType>
  void GetActorGeomPrimsAndStages(OmniConnectActorCache& actorCache, CacheType& volumeCache, double animTimeStep,
    PrimType& volVolume, PrimType& volGeom, PrimType& volTopGeom, UsdStageRefPtr& geomStage, UsdStageRefPtr& geomTopologyStage, bool& newVolume, bool& newTimeStep);
  void InitializeGeom(UsdGeomMesh& mesh, UsdStageRefPtr& geomStage, OmniConnectMeshCache& meshCache, bool setDefaultValues);
  void InitializeGeom(UsdGeomPointInstancer& points, UsdStageRefPtr& geomStage, OmniConnectInstancerCache& instancerCache, bool setDefaultValues);
  void InitializeGeom(UsdGeomPoints& points, UsdStageRefPtr& geomStage, OmniConnectInstancerCache& instancerCache, bool setDefaultValues);
  void InitializeGeom(UsdGeomBasisCurves& curves, UsdStageRefPtr& geomStage, OmniConnectCurveCache& curveCache, bool setDefaultValues);
  void InitializeGeom(UsdVolVolume& volume, UsdStageRefPtr& geomStage, OmniConnectVolumeCache& volumeCache, bool setDefaultValues);
  void InitializeGeom(UsdGeomMesh& volMesh, UsdStageRefPtr& geomStage, OmniConnectVolumeCache& volumeCache, bool setDefaultValues);
  UsdVolOpenVDBAsset InitializeVolumeField(UsdStageRefPtr& stage, const SdfPath& path, const TfToken& token, bool setDefaultValues) const;
  UsdVolOpenVDBAsset CreateOrRemoveOpenVDBAsset(UsdStageRefPtr& stage, const SdfPath& primPath, const TfToken& token, bool create, bool setDefaultValues) const;
  void ManageSetAndLinkOpenVDBAsset(UsdStageRefPtr& actorStage, UsdStageRefPtr& geomClipStage, UsdStageRefPtr& geomTopologyStage, const UsdVolVolume& volActGeom,
    const SdfPath& primPath, const TfToken& relToken, const TfToken& gridToken, bool createField, const SdfAssetPath& volAsset,
    bool performsUpdate, bool timeVaryingUpdate, const UsdTimeCode& timeCode);
  bool UpdateActorGeom(OmniConnectActorCache& actorCache, OmniConnectMeshCache& meshCache, const OmniConnectMeshData& omniMeshData, double animTimeStep,
    OmniConnectGenericArray* updatedGenericArrays, size_t numUga, OmniConnectGenericArray* deletedGenericArrays, size_t numDga);
  bool UpdateActorGeom(OmniConnectActorCache& actorCache, OmniConnectInstancerCache& instancerCache, OmniConnectInstancerData& omniInstancerData, double animTimeStep, 
    OmniConnectGenericArray* updatedGenericArrays, size_t numUga, OmniConnectGenericArray* deletedGenericArrays, size_t numDga);
  bool UpdateActorGeom(OmniConnectActorCache& actorCache, OmniConnectCurveCache& curveCache, const OmniConnectCurveData& omniCurveData, double animTimeStep,
    OmniConnectGenericArray* updatedGenericArrays, size_t numUga, OmniConnectGenericArray* deletedGenericArrays, size_t numDga);
  bool UpdateActorGeom(OmniConnectActorCache& actorCache, OmniConnectVolumeCache& volumeCache, OmniConnectVolumeData& omniVolumeData, double animTimeStep,
    OmniConnectGenericArray* updatedGenericArrays, size_t numUga, OmniConnectGenericArray* deletedGenericArrays, size_t numDga);
  void RemoveActorGeomUniformData(OmniConnectActorCache& actorCache, OmniConnectMeshCache& meshCache);
  void RemoveActorGeomUniformData(OmniConnectActorCache& actorCache, OmniConnectInstancerCache& instancerCache);
  void RemoveActorGeomUniformData(OmniConnectActorCache& actorCache, OmniConnectCurveCache& curveCache);
  void RemoveActorGeomUniformData(OmniConnectActorCache& actorCache, OmniConnectVolumeCache& volumeCache);
  void RemoveActorGeomVaryingData(OmniConnectActorCache& actorCache, OmniConnectMeshCache& meshCache, double animTimeStep);
  void RemoveActorGeomVaryingData(OmniConnectActorCache& actorCache, OmniConnectInstancerCache& instancerCache, double animTimeStep);
  void RemoveActorGeomVaryingData(OmniConnectActorCache& actorCache, OmniConnectCurveCache& curveCache, double animTimeStep);
  void RemoveActorGeomVaryingData(OmniConnectActorCache& actorCache, OmniConnectVolumeCache& volumeCache, double animTimeStep);
  template<typename CacheType> bool RemoveActorGeomAtTime(OmniConnectActorCache& actorCache, CacheType& geomCache, double animTimeStep);
  template<typename CacheType> void RemoveActorGeom(OmniConnectActorCache& actorCache, CacheType& geomCache);
  template<typename CacheType> void RemoveAllActorGeoms(OmniConnectActorCache& actorCache);
  void RemovePrimAndTopology(OmniConnectActorCache& actorCache, std::string& geomTopologyFilePath, SdfPath& primPath);
  void UpdateTransform(OmniConnectActorCache& actorCache, double* transform, double animTimeStep);
  template<typename CacheType> void SetMaterialBinding(OmniConnectActorCache& actorCache, size_t geomId, size_t matId);
  void SetTimeStepCodes(UsdStageRefPtr& stage, double newTime);
  UsdPrim FindOrCreateScenePrim(SdfPath& primPath, UsdStageRefPtr& sceneStage, std::string& clipPath);
  void DeleteSceneActor(OmniConnectActorCache& actorCache, UsdStageRefPtr& sceneStage);
  void CreateSceneActor(UsdStageRefPtr& sceneStage, OmniConnectActorCache& actorCache);
  size_t RetrieveSceneToAnimTimesFromUsd(const UsdPrim& actorPrim);
  void CopySceneToAnimTimes(double* sceneToAnimTimes, size_t numSceneToAnimTimes);
  template<typename CacheType> void RestoreClipActivesAndPaths(const OmniConnectActorCache& actorCache, CacheType& geomCache);
  void RetimeSceneToAnimClips(const VtVec2dArray& sceneClipTimes, const VtVec2dArray& animClipActives, VtVec2dArray& newAnimClipActives);
  template<typename CacheType> void RetimeActorGeoms(OmniConnectActorCache& actorCache, UsdStageRefPtr& scenestage, const VtVec2dArray& sceneClipTimes, VtVec2dArray& newAnimClipActives);
  void SetClipValues(OmniConnectActorCache& actorCache, UsdStageRefPtr& scenestage, UsdPrim& actorPrim, 
    const double* sceneToAnimTimes, size_t numSceneToAnimTimes);
#ifdef FORCE_OMNI_CLIP_UPDATES_WITH_DUMMY
  void AttachAsSublayer(UsdStageRefPtr& stage, std::string& fileName, bool attach);
#endif
  void SetActorVisibility(OmniConnectActorCache& actorCache, bool visible, double animTimeStep);
  template<typename CacheType> void SetGeomVisibility(OmniConnectActorCache& actorCache, size_t geomId, bool visible, double animTimeStep);

  OmniConnectActorCache* GetCachedActorCache(size_t actorId);

  // Live workflow
  void SetLiveWorkflowEnabled(bool enable);

  class ForceLiveWorkflowDisabled
  {
    public:
      ForceLiveWorkflowDisabled(OmniConnectInternals* internals)
        : Internals(internals)
      {
        WorkflowEnabled = Internals->LiveInterface.GetLiveWorkflowEnabled();
        if(WorkflowEnabled)
          OmniConnectDebugMacro("Temporarily disabling and re-enabling live workflow to complete operation, any active live sessions in other applications will be interrupted and have to be reloaded.");

        Internals->SetLiveWorkflowEnabled(false);
      }

      ~ForceLiveWorkflowDisabled()
      {
        Internals->SetLiveWorkflowEnabled(WorkflowEnabled);
      }

    private:
      OmniConnectInternals* Internals = nullptr;
      bool WorkflowEnabled = false;
  };

  // Logging
  static OmniConnectLogCallback LogCallback;

  std::unique_ptr<OmniConnectDiagnosticMgrDelegate> DiagnosticDelegate;
  std::function<void (OmniConnectDiagnosticMgrDelegate*)> DiagRemoveFunc;

  // Omniverse Connect interface
  OmniConnectConnection* Connection;

  // Volume Writer interface
  std::shared_ptr<OmniConnectVolumeWriterI> VolumeWriter; // shared - requires custom deleter

  // USD Live interface
  OmniConnectLiveInterface LiveInterface;

  // Allow for writeback to usd/omniverse
  bool UsdSaveEnabled = true;

  // Convert generic arrays from double to float
  bool ConvertGenericArraysDoubleToFloat = false;

  // Settings and caches
  OmniConnectSettings Settings;
  OmniConnectEnvironment Environment;
  ActorCacheMapType ActorCacheMap;

  // Session specific info
  int SessionNumber = -1;
  std::string UsdExtension;
  std::string SceneFileName;
  std::string MultiSceneFileName;
  std::string RootLevelFileName;
  std::string SessionDirectory;
  std::string SceneDirectory;
  UsdStageRefPtr RootStage;
  UsdStageRefPtr MultiSceneStage;
  std::string MultiSceneStageUrl;
  UsdStageRefPtr SceneStage;
  std::string SceneStageUrl;

  std::string RootPrimName;
  std::string ActScopeName;
  std::string MatScopeName;
  std::string TexScopeName;
  std::string LightScopeName;
  SdfPath SdfRootPrimName;
  SdfPath SdfActScopeName;
  SdfPath SdfMatScopeName;
  SdfPath SdfTexScopeName;
  SdfPath SdfLightScopeName;

#ifdef USE_MDL_MATERIALS
  OmniConnectMdlNames MdlNames;
#endif

  // Temp arrays
  std::vector<size_t> TempIdVec;
  VtVec2dArray TempSceneToAnimTimes;

  // Cached OmniConnectActorCache pointer
  size_t CurrentCachedActorId = size_t(-1);
  OmniConnectActorCache* CachedActorCache;

#ifdef FORCE_OMNI_CLIP_UPDATES_WITH_DUMMY
  std::vector<UsdStageRefPtr> DummyStages;
  std::vector<std::string> DummyFiles; // includes dummy subdirectory
  std::vector<std::string> DummyFilePaths; // complete relative path from working dir
  int RunningDummyIndex = -1;
  static const int NumDummyStages;
#endif
};

OmniConnectLogCallback OmniConnectInternals::LogCallback = nullptr;

#ifdef FORCE_OMNI_CLIP_UPDATES_WITH_DUMMY
const int OmniConnectInternals::NumDummyStages = 11;
#endif

int OmniConnectInternals::FindSessionNumber()
{
  int maxSessionNr = Connection->MaxSessionNr();

  maxSessionNr = std::max(0, maxSessionNr + Settings.CreateNewOmniSession); //Increase maximum found session number by one, or use last created session.
 
  return maxSessionNr;
}

void OmniConnectInternals::InitializeSession()
{
  bool hasRootFileName = this->Settings.RootLevelFileName && STRNLEN_PORTABLE(this->Settings.RootLevelFileName, MAX_USER_STRLEN) > 0;

  this->SessionNumber = FindSessionNumber();

  this->SessionDirectory = (hasRootFileName ? this->Settings.RootLevelFileName : ("Session_" + std::to_string(this->SessionNumber))) + "/";

  this->UsdExtension = this->Settings.OutputBinary ? ".usd" : ".usda";
  
  bool folderMayExist = !this->Settings.CreateNewOmniSession;

  Connection->CreateFolder("", true);

  if (this->Environment.ProcId == 0)
  {
    //Connection->RemoveFolder(this->SessionDirectory.c_str()); 
    Connection->CreateFolder(this->SessionDirectory.c_str(), folderMayExist);
    if(this->Environment.NumProcs > 1)
      OmniConnectDebugMacro("Initializing multiprocess session --- Main proc session folder creation done.");
  }

  if (Environment.NumProcs == 1)
  {
    this->SceneDirectory = this->SessionDirectory;
  }
  else
  {
    this->SceneDirectory += this->SessionDirectory + "Proc_" + std::to_string(this->Environment.ProcId) + "/";
    Connection->CreateFolder(this->SceneDirectory.c_str(), folderMayExist);
    OmniConnectDebugMacro("Initializing multiprocess session --- Multiprocess session, procId: " << this->Environment.ProcId)
  }

  std::string matDirectory = this->SceneDirectory + OmniMaterialRelativePath;
  Connection->CreateFolder(matDirectory.c_str(), folderMayExist);
  std::string geomDirectory = this->SceneDirectory + OmniGeomRelativePath;
  Connection->CreateFolder(geomDirectory.c_str(), folderMayExist);
  std::string topDirectory = this->SceneDirectory + OmniTopologyRelativePath;
  Connection->CreateFolder(topDirectory.c_str(), folderMayExist);
  std::string texDirectory = this->SceneDirectory + OmniTextureRelativePath;
  Connection->CreateFolder(texDirectory.c_str(), folderMayExist);
  std::string volDirectory = this->SceneDirectory + OmniVolumeRelativePath;
  Connection->CreateFolder(volDirectory.c_str(), folderMayExist);
#ifdef FORCE_OMNI_CLIP_UPDATES_WITH_DUMMY
  std::string dummyLayerDirectory = this->SceneDirectory + OmniDummyLayerPath;
  Connection->CreateFolder(dummyLayerDirectory.c_str(), folderMayExist);
#endif

  this->RootPrimName = "/Root";
  this->ActScopeName = this->RootPrimName + "/Actors";
  this->MatScopeName = this->RootPrimName + "/Looks";
  this->TexScopeName = this->RootPrimName + "/Textures";
  this->LightScopeName = this->RootPrimName + "/Lights";
  this->SdfRootPrimName = SdfPath(this->RootPrimName);
  this->SdfActScopeName = SdfPath(this->ActScopeName);
  this->SdfMatScopeName = SdfPath(this->MatScopeName);
  this->SdfTexScopeName = SdfPath(this->TexScopeName);
  this->SdfLightScopeName = SdfPath(this->LightScopeName);
  this->SceneFileName = "FullScene" + this->UsdExtension;
  this->MultiSceneFileName = "MultiScene" + this->UsdExtension;
  if(hasRootFileName)
    this->RootLevelFileName = this->Settings.RootLevelFileName + this->UsdExtension;

  LiveInterface.Initialize(this->Connection, OmniConnectInternals::LogCallback, &this->UsdExtension, &this->RootPrimName);
}

void OmniConnectInternals::OpenRootLevelStage()
{
  if(this->RootLevelFileName.empty())
    return;

  // Put the file into the root (working) directory of the connection
  const char* stageUrl = this->Connection->GetUrl(this->RootLevelFileName.c_str());

  if(!this->Settings.CreateNewOmniSession)
    this->RootStage = UsdStage::Open(stageUrl);

  if (!this->RootStage)
  {
    this->RootStage = UsdStage::CreateNew(stageUrl);
    assert(this->RootStage);
  }

  SdfLayerHandle rootSubLayer = MultiSceneStage ? MultiSceneStage->GetRootLayer() : SceneStage->GetRootLayer();
  this->RootStage->GetRootLayer()->InsertSubLayerPath(rootSubLayer->GetIdentifier());

  if (UsdSaveEnabled)
  {
    this->RootStage->Save();
  }
}

void OmniConnectInternals::OpenMultiSceneStage()
{
  std::string relScenePath = this->SessionDirectory + this->MultiSceneFileName;
  const char* stageUrl = this->Connection->GetUrl(relScenePath.c_str());
  this->MultiSceneStageUrl = stageUrl;
  
  if(!this->Settings.CreateNewOmniSession)
    this->MultiSceneStage = UsdStage::Open(stageUrl);

  if (!this->MultiSceneStage)
  {
    this->MultiSceneStage = UsdStage::CreateNew(stageUrl);
    assert(this->MultiSceneStage);
  }

  UsdPrim rootPrim = UsdGeomXform::Define(this->MultiSceneStage, this->SdfRootPrimName).GetPrim();
  assert(rootPrim);
  this->MultiSceneStage->SetDefaultPrim(rootPrim);
  UsdModelAPI(rootPrim).SetKind(KindTokens->assembly);

  CreateDefaultLighting(this->MultiSceneStage);

  for (int i = 0; i < this->Environment.NumProcs; ++i)
  {
    std::string procSubLayerPath = "Proc_" + std::to_string(i) + "/" + this->SceneFileName;
    this->MultiSceneStage->GetRootLayer()->InsertSubLayerPath(procSubLayerPath);
  }

  if (UsdSaveEnabled)
  {
    this->MultiSceneStage->Save();
  }
}

void OmniConnectInternals::OpenSceneStage()
{
  std::string relScenePath = this->SceneDirectory + this->SceneFileName;
  const char* stageUrl = this->Connection->GetUrl(relScenePath.c_str());
  this->SceneStageUrl = stageUrl;

  if(!this->Settings.CreateNewOmniSession)
    this->SceneStage = UsdStage::Open(stageUrl);

  if (!this->SceneStage)
  {
    this->SceneStage = UsdStage::CreateNew(stageUrl);
    assert(this->SceneStage);
  }

  UsdPrim rootPrim = UsdGeomXform::Define(this->SceneStage, this->SdfRootPrimName).GetPrim();
  assert(rootPrim);
  this->SceneStage->SetDefaultPrim(rootPrim);
  UsdModelAPI(rootPrim).SetKind(KindTokens->assembly);

  UsdGeomSetStageUpAxis(this->SceneStage,
    (this->Settings.UpAxis == OmniConnectAxis::Y) ? UsdGeomTokens->y : UsdGeomTokens->z);

#ifndef TOPOLOGY_AS_SUBLAYER
  UsdPrim matScopePrim = this->SceneStage->OverridePrim(this->SdfMatScopeName);
  assert(matScopePrim);
#endif

  if (this->Environment.NumProcs == 1)
  {
    CreateDefaultLighting(this->SceneStage);
  }

  if (UsdSaveEnabled)
  {
    this->SceneStage->Save();
  }

#ifdef FORCE_OMNI_CLIP_UPDATES_WITH_DUMMY
  this->DummyFiles.resize(this->NumDummyStages);
  this->DummyStages.resize(this->NumDummyStages);
  this->DummyFilePaths.resize(this->NumDummyStages);
  for(int i = 0; i < this->NumDummyStages; ++i)
  {
    this->DummyFiles[i] = std::string(OmniDummyLayerPath) + "Dummy" + std::to_string(i) + this->UsdExtension;
    this->DummyFilePaths[i] = this->SceneDirectory + this->DummyFiles[i];
    const char* dummyStageUrl = this->Connection->GetUrl(this->DummyFilePaths[i].c_str());

    if(!this->Settings.CreateNewOmniSession)
      this->DummyStages[i] = UsdStage::Open(dummyStageUrl);

    if(!this->DummyStages[i])
    {
      this->DummyStages[i] = UsdStage::CreateNew(dummyStageUrl);

      auto& curDummyStage = this->DummyStages[i];
      UsdPrim dummyRootPrim = UsdGeomXform::Define(curDummyStage, this->SdfRootPrimName).GetPrim();
      assert(dummyRootPrim);
      curDummyStage->SetDefaultPrim(dummyRootPrim);
      UsdModelAPI(dummyRootPrim).SetKind(KindTokens->assembly);

      if(UsdSaveEnabled)
        curDummyStage->Save();
    }
  }
#endif

#ifdef USE_MDL_MATERIALS
  CreateMdlTemplates(Connection, this->MdlNames, this->SceneDirectory, OmniMaterialRelativePath);
#endif

  Connection->ProcessUpdates();
}

void OmniConnectInternals::CreateDefaultLighting(UsdStageRefPtr & stage)
{
  SdfPath lightPath = this->SdfLightScopeName.AppendPath(SdfPath("defaultLight"));
  UsdLuxDistantLight defaultLight = UsdLuxDistantLight::Define(stage, lightPath);

  UsdLuxShapingAPI shapingAPI(defaultLight);

  defaultLight.CreateAngleAttr().Set(1.0f);
  defaultLight.CreateIntensityAttr().Set(1500.0f);

  GfVec3d translate(0, 0, 0);
  GfVec3f rotate(315, 0, 0);
  GfVec3f scale(1, 1, 1);
  defaultLight.ClearXformOpOrder();
  defaultLight.AddTranslateOp().Set(translate);
  defaultLight.AddRotateXYZOp().Set(rotate);
  defaultLight.AddScaleOp().Set(scale);

  shapingAPI.CreateShapingConeAngleAttr().Set(180.0f);
  shapingAPI.CreateShapingConeSoftnessAttr();
  shapingAPI.CreateShapingFocusAttr();
}

void OmniConnectInternals::OpenActorStage(OmniConnectActorCache& actorCache)
{
  // Create an anonymous layer to populate (file name determined by export to layer in Export() later on).

  // Create a UsdStage with that root layer (file name determined here, used at Save()).
  const char* stageUrl = this->Connection->GetUrl(actorCache.UsdOutputFilePath.c_str());
  actorCache.Stage = UsdStage::CreateNew(stageUrl); // Always try to create a new stage first, since a new actor may be created regardless of whether createnewsession or not
  actorCache.UsdOutputFileUrl = stageUrl;

  //In case the stage already existed or didn't get deleted properly, it can be opened again. 
  if (!actorCache.Stage) 
  {
    actorCache.Stage = UsdStage::Open(stageUrl);
    assert(actorCache.Stage);
  }

  UsdPrim rootPrim = UsdGeomXform::Define(actorCache.Stage, this->SdfRootPrimName).GetPrim();
  assert(rootPrim);

  UsdPrim actorsPrim = UsdGeomXform::Define(actorCache.Stage, this->SdfActScopeName).GetPrim();
  UsdModelAPI(actorsPrim).SetKind(KindTokens->group);
  assert(actorsPrim);

  actorCache.Stage->SetDefaultPrim(rootPrim);
  //SdfPrimSpecHandle rootPrimSpec = rootPrim.GetPrimDefinition();
  UsdModelAPI(rootPrim).SetKind(KindTokens->assembly);

  UsdGeomXform actorXform = UsdGeomXform::Define(actorCache.Stage, actorCache.SdfActorPrimPath);
  UsdModelAPI(actorXform.GetPrim()).SetKind(KindTokens->component);
}

void OmniConnectInternals::OpenClipAndTopologyStage(const char* clipFilePath, const char* topologyFilePath, UsdStageRefPtr& clipStage, UsdStageRefPtr& topologyStage)
{
  const char* stageUrl = this->Connection->GetUrl(clipFilePath);
  
  OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(false);
  clipStage = UsdStage::Open(stageUrl); // Try to open first: clip stage timestep may be revisited, or process may have been killed earlier
  OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(true);

  if (!clipStage)
  {
    clipStage = UsdStage::CreateNew(stageUrl);
    assert(clipStage);
  }

  stageUrl = this->Connection->GetUrl(topologyFilePath);

  OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(false);
  topologyStage = UsdStage::Open(stageUrl);
  OmniConnectDiagnosticMgrDelegate::SetOutputEnabled(true);

  if (!topologyStage)
  {
    topologyStage = UsdStage::CreateNew(stageUrl);
    assert(topologyStage);  
  }
}

OmniConnectActorCache* OmniConnectInternals::GetCachedActorCache(size_t actorId)
{
  if (this->CurrentCachedActorId != actorId)
  {
    auto actorCacheIt = this->ActorCacheMap.find(actorId);
    assert(actorCacheIt != this->ActorCacheMap.end());
    this->CachedActorCache = &actorCacheIt->second;
    actorId = this->CurrentCachedActorId;
  }
  return this->CachedActorCache;
}

UsdShadeOutput OmniConnectInternals::CreateUsdPreviewSurface(OmniConnectActorCache& actorCache, OmniConnectMatCache& matCache, UsdShadeShader& shader, bool newMat)
{
  shader.CreateIdAttr(VtValue(OmniConnectTokens->UsdPreviewSurface));
  shader.CreateInput(OmniConnectTokens->useSpecularWorkflow, SdfValueTypeNames->Int).Set(0);
  shader.CreateInput(OmniConnectTokens->roughness, SdfValueTypeNames->Float);
  shader.CreateInput(OmniConnectTokens->opacity, SdfValueTypeNames->Float);
  shader.CreateInput(OmniConnectTokens->diffuseColor, SdfValueTypeNames->Color3f);
  shader.CreateInput(OmniConnectTokens->specularColor, SdfValueTypeNames->Color3f);
  shader.CreateInput(OmniConnectTokens->metallic, SdfValueTypeNames->Float);
  shader.CreateInput(OmniConnectTokens->ior, SdfValueTypeNames->Float);
  shader.CreateInput(OmniConnectTokens->emissiveColor, SdfValueTypeNames->Color3f);

  // Create a standard texture coordinate reader
  UsdShadeShader texCoordReader = UsdDefineOrGet<UsdShadeShader>(actorCache.Stage, matCache.TexCoordReaderPath, newMat);
  assert(texCoordReader);
  texCoordReader.CreateIdAttr(VtValue(OmniConnectTokens->PrimStId));
  texCoordReader.CreateInput(OmniConnectTokens->varname, SdfValueTypeNames->Token).Set(TEX_PRIMVAR_NAME);

  // Create a vertexcolorreader
  UsdShadeShader vertexColorReader = UsdDefineOrGet<UsdShadeShader>(actorCache.Stage, matCache.VertexColorReaderPath, newMat);
  assert(vertexColorReader);
  vertexColorReader.CreateIdAttr(VtValue(OmniConnectTokens->PrimDisplayColorId));
  vertexColorReader.CreateInput(OmniConnectTokens->varname, SdfValueTypeNames->Token).Set(OmniConnectTokens->displayColor);

  // Create a texture reader
  UsdShadeShader textureReader = UsdDefineOrGet<UsdShadeShader>(actorCache.Stage, matCache.TextureReaderPath, newMat);
  assert(textureReader);
  UsdShadeOutput tcReaderOutput = texCoordReader.CreateOutput(OmniConnectTokens->result, SdfValueTypeNames->Float2);
  textureReader.CreateInput(OmniConnectTokens->st, SdfValueTypeNames->Float2).ConnectToSource(tcReaderOutput);

  // Create output
  return shader.CreateOutput(OmniConnectTokens->surface, SdfValueTypeNames->Token);
}

void OmniConnectInternals::ResetUsdPreviewSurface(UsdShadeShader& shader)
{
  shader.GetInput(OmniConnectTokens->diffuseColor).GetAttr().Clear();
  shader.GetInput(OmniConnectTokens->specularColor).GetAttr().Clear();
  shader.GetInput(OmniConnectTokens->roughness).GetAttr().Clear();
  shader.GetInput(OmniConnectTokens->opacity).GetAttr().Clear(); 
  shader.GetInput(OmniConnectTokens->metallic).GetAttr().Clear();
  shader.GetInput(OmniConnectTokens->ior).GetAttr().Clear();
  shader.GetInput(OmniConnectTokens->emissiveColor).GetAttr().Clear();
}

void OmniConnectInternals::UpdateUsdPreviewSurface(OmniConnectActorCache& actorCache, OmniConnectMatCache& matCache, const OmniConnectTexCache* texCache, const OmniConnectMaterialData& omniMatData, UsdShadeShader& shader, double animTimeStep)
{
  TimeEvaluator<OmniConnectMaterialData> timeEval(omniMatData, animTimeStep);
  typedef OmniConnectMaterialData::DataMemberId DMI;

  GfVec3f difColor(omniMatData.Diffuse); ocutils::SrgbToLinear3(&difColor[0]);
  GfVec3f specColor(omniMatData.Specular); ocutils::SrgbToLinear3(&specColor[0]); // Not sure yet how to incorporate the specular color, it doesn't directly map to usd specularColor
  GfVec3f emColor(omniMatData.Emissive); ocutils::SrgbToLinear3(&emColor[0]);
  emColor *= omniMatData.EmissiveIntensity;

  shader.GetInput(OmniConnectTokens->roughness).Set(omniMatData.Roughness, timeEval.Eval(DMI::ROUGHNESS));
  shader.GetInput(OmniConnectTokens->opacity).Set(omniMatData.Opacity, timeEval.Eval(DMI::OPACITY));
  shader.GetInput(OmniConnectTokens->metallic).Set(omniMatData.Metallic, timeEval.Eval(DMI::METALLIC));
  shader.GetInput(OmniConnectTokens->ior).Set(omniMatData.Ior, timeEval.Eval(DMI::IOR));
  shader.GetInput(OmniConnectTokens->emissiveColor).Set(emColor, timeEval.Eval(DMI::EMISSIVE));

  if (omniMatData.TexId == -1)
  {
    if (omniMatData.UseVertexColors)
    {
      UsdShadeShader vertexColorReader = UsdShadeShader::Get(actorCache.Stage, matCache.VertexColorReaderPath);
      assert(vertexColorReader);

      UsdShadeOutput vcReaderOutput = vertexColorReader.CreateOutput(OmniConnectTokens->result, SdfValueTypeNames->Color3f);

      shader.GetInput(OmniConnectTokens->diffuseColor).ConnectToSource(vcReaderOutput);
      shader.GetInput(OmniConnectTokens->specularColor).ConnectToSource(vcReaderOutput);
    }
    else
    {
      UsdShadeInput diffInput = shader.GetInput(OmniConnectTokens->diffuseColor);
      UsdShadeInput specInput = shader.GetInput(OmniConnectTokens->specularColor);
      diffInput.DisconnectSource();
      diffInput.Set(difColor, timeEval.Eval(DMI::DIFFUSE));
      specInput.DisconnectSource();
      specInput.Set(specColor, timeEval.Eval(DMI::SPECULAR));
    }
  }
  else
  {
    // Find the coordinate reader using the material path
    UsdShadeShader coordReaderPrim = UsdShadeShader::Get(actorCache.Stage, matCache.TexCoordReaderPath);
    assert(coordReaderPrim);

    // Set the texture reader to reference an actual texture
    UsdShadeShader refTexReader = UsdShadeShader::Get(actorCache.Stage, matCache.TextureReaderPath); // type inherited from texture prim (in AddRef)
    assert(refTexReader);
    refTexReader.GetPrim().GetReferences().AddInternalReference(texCache->TexturePrimPath);

    UsdShadeOutput texReaderOutput = refTexReader.CreateOutput(OmniConnectTokens->rgb, SdfValueTypeNames->Color3f);

    //Bind the texture to the diffuse color of the shader
    shader.GetInput(OmniConnectTokens->diffuseColor).ConnectToSource(texReaderOutput);
    shader.GetInput(OmniConnectTokens->specularColor).ConnectToSource(texReaderOutput);
  }
}

void OmniConnectInternals::UpdateActorMaterial(OmniConnectActorCache& actorCache
  , const OmniConnectMaterialData& omniMatData
  , double animTimeStep
  )
{
  size_t matId = omniMatData.MaterialId;

  auto matCachePair = actorCache.GetMatCache(matId);
  UsdStageRefPtr& stage = actorCache.Stage;
  
  OmniConnectMatCache& matCache = matCachePair.second;
  bool newMatCache = matCachePair.first;

  bool forceTimeVarying = omniMatData.TimeVarying == OmniConnectMaterialData::DataMemberId::ALL;
  if(newMatCache)
  {
    matCache.TimeVarying = forceTimeVarying;
  }

  UsdShadeMaterial material = UsdShadeMaterial::Get(stage, matCache.SdfMatName);
  bool newMat = !material;
  if (newMat)
  {
    material = UsdShadeMaterial::Define(stage, matCache.SdfMatName);
  }
  assert(material);

  UsdShadeShader shader = UsdDefineOrGet<UsdShadeShader>(stage, matCache.ShadPath, newMat);
  assert(shader);

#ifdef USE_MDL_MATERIALS
  UsdShadeShader mdlShader = UsdDefineOrGet<UsdShadeShader>(stage, matCache.MdlShadPath, newMat);
  assert(mdlShader);
#endif

#ifdef USE_INDEX_MATERIALS
  UsdShadeShader indexShader = UsdDefineOrGet<UsdShadeShader>(stage, matCache.IndexShadPath, newMat);
  assert(indexShader);
#endif

  if (newMat)
  {
    if(!omniMatData.VolumeMaterial)
    {
      UsdShadeOutput shaderOutput = CreateUsdPreviewSurface(actorCache, matCache, shader, newMat);   
      material.CreateSurfaceOutput().ConnectToSource(shaderOutput);
    }

#ifdef USE_MDL_MATERIALS
    if (!omniMatData.VolumeMaterial)
    {
      UsdShadeOutput mdlShaderOutput = CreateUsdMdlSurfaceShader(actorCache, matCache, mdlShader, this->MdlNames, newMat);
      material.CreateSurfaceOutput(OmniConnectTokens->mdl).ConnectToSource(mdlShaderOutput);
    }
    else
    {
      UsdShadeOutput mdlShaderOutput = CreateUsdMdlVolumeShader(mdlShader);
      material.CreateVolumeOutput(OmniConnectTokens->mdl).ConnectToSource(mdlShaderOutput);
    }
#endif

#ifdef USE_INDEX_MATERIALS
    if (omniMatData.VolumeMaterial)
    {
      UsdShadeOutput indexShaderOutput = CreateUsdIndexVolumeShader(actorCache, matCache, indexShader, newMat);
      material.CreateVolumeOutput(OmniConnectTokens->nvindex).ConnectToSource(indexShaderOutput);
    }
#endif

    material.GetPrim().CreateAttribute(OmniConnectTokens->MatId, SdfValueTypeNames->UInt64).Set(matId);
  }

  const OmniConnectTexCache* texCache = (omniMatData.TexId != -1) ? &actorCache.GetTexCache(omniMatData.TexId).second : nullptr;

  if (matCache.TimeVarying != forceTimeVarying)
  {
    if(!omniMatData.VolumeMaterial)
    {
      ResetUsdPreviewSurface(shader);
#ifdef USE_MDL_MATERIALS
      ResetUsdMdlShader(mdlShader, actorCache, matCache);
#endif
    }
    else
    {
#ifdef USE_INDEX_MATERIALS
      ResetUsdIndexVolumeShader(actorCache, matCache);
#endif
    }
    matCache.TimeVarying = forceTimeVarying;
  }

  if(!omniMatData.VolumeMaterial)
  {
    UpdateUsdPreviewSurface(actorCache, matCache, texCache, omniMatData, shader, animTimeStep);
#ifdef USE_MDL_MATERIALS
    UpdateUsdMdlShader(actorCache, matCache, texCache, omniMatData, mdlShader, this->MdlNames, animTimeStep);
#endif
  }
  else
  {
#ifdef USE_INDEX_MATERIALS
    UpdateUsdIndexVolumeShader(actorCache, matCache, texCache, omniMatData, indexShader, animTimeStep);
#endif    
  }
}


void OmniConnectInternals::RemoveActorMaterial(OmniConnectActorCache& actorCache, size_t matId)
{
  OmniConnectMatCache& matCache = actorCache.GetMatCache(matId).second;

  actorCache.Stage->RemovePrim(matCache.SdfMatName);
}

UsdShadeShader OmniConnectInternals::InitializeUsdTexture(OmniConnectActorCache& actorCache, OmniConnectSamplerData& samplerData, OmniConnectTexCache& texCache)
{
  UsdShadeShader textureReader = UsdShadeShader::Define(actorCache.Stage, texCache.TexturePrimPath);
  assert(textureReader);
  textureReader.CreateIdAttr(VtValue(OmniConnectTokens->UsdUVTexture));
  textureReader.CreateInput(OmniConnectTokens->fallback, SdfValueTypeNames->Float4).Set(GfVec4f(1.0f, 0.0f, 0.0f, 1.0f));
  textureReader.CreateOutput(OmniConnectTokens->rgb, SdfValueTypeNames->Float3);
  textureReader.CreateOutput(OmniConnectTokens->a, SdfValueTypeNames->Float);
  textureReader.CreateInput(OmniConnectTokens->file, SdfValueTypeNames->Asset).Set(texCache.SceneRelTexturePath);

  // Properties determined at texture creation, remain static
  textureReader.CreateInput(OmniConnectTokens->WrapS, SdfValueTypeNames->Token).Set(TextureWrapToken(samplerData.WrapS));
  textureReader.CreateInput(OmniConnectTokens->WrapT, SdfValueTypeNames->Token).Set(TextureWrapToken(samplerData.WrapT));

  return textureReader;
}

void OmniConnectInternals::UpdateUsdTexture(OmniConnectTexCache& texCache, UsdShadeShader& textureReader, bool timeVarying, double animTimeStep)
{
  UsdShadeInput fileInput = textureReader.GetInput(OmniConnectTokens->file);
  if (texCache.TimeVarying != timeVarying)
  {
    // Only clear the attribute, leave files (with associated timesteps in cache) alone
    fileInput.GetAttr().Clear();
    texCache.TimeVarying = timeVarying;

    if(!timeVarying)
      fileInput.Set(texCache.SceneRelTexturePath); // filename doesn't change if content changes, so only has to be set once for the default timestep (it was initialized in InitializeUsdTexture())
  }

  if (timeVarying)
  {
    fileInput.Set(texCache.TimedSceneRelTexturePath, animTimeStep);
  }
}

void OmniConnectInternals::UpdateTexture(OmniConnectActorCache& actorCache, size_t texId, OmniConnectSamplerData& samplerData, bool timeVarying, double animTimeStep)
{
  auto texCachePair = actorCache.GetTexCache(texId);

  OmniConnectTexCache& texCache = texCachePair.second;
  bool newTexCache = texCachePair.first;

  // Can be moved to initialization, but this is fine too
  if(newTexCache)
  {
    texCache.WrapS = samplerData.WrapS;
    texCache.WrapT = samplerData.WrapT;
    texCache.TimeVarying = timeVarying;
  }

  if (timeVarying)
  {
    texCache.ResetTimedTexPaths(animTimeStep);
    if (std::find(texCache.TimeSteps.begin(), texCache.TimeSteps.end(), animTimeStep) == texCache.TimeSteps.end())
      texCache.TimeSteps.push_back(animTimeStep);
  }
  std::string& texTargetPath = timeVarying ? texCache.TimedAbsTexturePath : texCache.AbsTexturePath;
  Connection->WriteFile(samplerData.TexData, samplerData.TexDataSize, texTargetPath.c_str());

  UsdShadeShader textureReader = UsdShadeShader::Get(actorCache.Stage, texCache.TexturePrimPath);
  if(!textureReader)
    textureReader = InitializeUsdTexture(actorCache, samplerData, texCache);

  UpdateUsdTexture(texCache, textureReader, timeVarying, animTimeStep); // updates texCache.timeVarying

#if !USE_CUSTOM_MDL
  UsdShadeShader mdlSampler = UsdShadeShader::Get(actorCache.Stage, texCache.TexturePrimPath_mdl);
  if(!mdlSampler)
    mdlSampler = InitializeMdlTexture(actorCache, samplerData, texCache);

  UpdateMdlTexture(texCache, mdlSampler, animTimeStep); // uses texCache.timeVarying
#endif
}

void OmniConnectInternals::DeleteTexture(OmniConnectActorCache& actorCache
  , size_t texCacheIdx 
)
{
  auto& texCacheEntry = actorCache.TexCaches[texCacheIdx];
  OmniConnectTexCache& texCache = texCacheEntry.second;

  // Remove the default texture file
  Connection->RemoveFile(texCache.AbsTexturePath.c_str());

  // For each timestep-texture ever written, remove the file
  for (double timeStep : texCache.TimeSteps)
  {
    texCache.ResetTimedTexPaths(timeStep);
    Connection->RemoveFile(texCache.TimedAbsTexturePath.c_str());
  }

  actorCache.Stage->RemovePrim(texCache.TexturePrimPath);
}

void OmniConnectInternals::UpdateGenericArrays(UsdGeomPrimvarsAPI& actorPrimvarsApi, UsdGeomPrimvarsAPI& clipPrimvarsApi, UsdGeomPrimvarsAPI& topPrimvarsApi, 
  OmniConnectGenericArray* genericArrays, size_t numGenericArrays, double animTimeStep)
{
  TimeEvaluator<bool> timeEval(false, animTimeStep);

  for (int i = 0; i < numGenericArrays; ++i)
  {
    OmniConnectType dataType = genericArrays[i].DataType;
    assert(dataType != OmniConnectType::UNDEFINED);
    const char* primvarName = genericArrays[i].Name;
    void* arrayData = genericArrays[i].Data;
    size_t arrayNumElements = genericArrays[i].NumElements;
    bool perPoly = genericArrays[i].PerPoly;
    timeEval.TimeVarying = genericArrays[i].TimeVarying;
    const UsdTimeCode& timeCode = timeEval.Eval();
    bool setPrimvar = true;

    switch (dataType)
    {
      case OmniConnectType::UCHAR: {GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->UCharArray); ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtUCharArray); break; }
      case OmniConnectType::CHAR: {GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->UCharArray); ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtUCharArray); break; }
      case OmniConnectType::USHORT: {GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->UIntArray); ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtUIntArray, short); break; }
      case OmniConnectType::SHORT: {GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->IntArray); ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtIntArray, unsigned short); break; }
      case OmniConnectType::UINT: {GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->UIntArray); ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtUIntArray); break; }
      case OmniConnectType::INT: {GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->IntArray); ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtIntArray); break; }
      case OmniConnectType::LONG: {GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->Int64Array); ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtInt64Array); break; }
      case OmniConnectType::ULONG: {GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->UInt64Array); ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtUInt64Array); break; }
      case OmniConnectType::FLOAT: {GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->FloatArray); ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtFloatArray); break; }
      case OmniConnectType::DOUBLE: 
      {
        if(ConvertGenericArraysDoubleToFloat)
        {
          GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->FloatArray); ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtFloatArray, double);
        }
        else
        {
          GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->DoubleArray); ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtDoubleArray);
        }
        break;
      }

      case OmniConnectType::INT2: {GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->Int2Array); ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec2iArray); break; }
      case OmniConnectType::FLOAT2: {GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->Float2Array); ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec2fArray); break; }
      case OmniConnectType::DOUBLE2:
      {
        if(ConvertGenericArraysDoubleToFloat)
        {
          GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->Float2Array); ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtVec2fArray, GfVec2d);
        }
        else
        {
          GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->Double2Array); ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec2dArray);
        }
        break;
      }

      case OmniConnectType::INT3: {GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->Int3Array); ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec3iArray); break; }
      case OmniConnectType::FLOAT3: {GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->Float3Array); ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec3fArray); break; }
      case OmniConnectType::DOUBLE3:
      {
        if(ConvertGenericArraysDoubleToFloat)
        {
          GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->Float3Array); ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtVec3fArray, GfVec3d);
        }
        else
        {
          GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->Double3Array); ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec3dArray);
        }
        break;
      }

      case OmniConnectType::INT4: {GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->Int4Array); ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec4iArray); break; }
      case OmniConnectType::FLOAT4: {GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->Float4Array); ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec4fArray); break; }
      case OmniConnectType::DOUBLE4:
      {
        if(ConvertGenericArraysDoubleToFloat)
        {
          GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->Float4Array); ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtVec4fArray, GfVec4d);
        }
        else
        {
          GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->Double4Array); ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec4dArray);
        }
        break;
      }

      case OmniConnectType::UCHAR2:
      case OmniConnectType::UCHAR3: 
      case OmniConnectType::UCHAR4: {GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->UCharArray); ASSIGN_ARRAY_TO_PRIMVAR_FLATTEN_MACRO(VtUCharArray); break; }
      case OmniConnectType::CHAR2:
      case OmniConnectType::CHAR3: 
      case OmniConnectType::CHAR4: {GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->UCharArray); ASSIGN_ARRAY_TO_PRIMVAR_FLATTEN_MACRO(VtUCharArray); break; }
      case OmniConnectType::USHORT2:
      case OmniConnectType::USHORT3:
      case OmniConnectType::USHORT4: {GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->UIntArray); ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_FLATTEN_MACRO(VtUIntArray, short); break; }
      case OmniConnectType::SHORT2:
      case OmniConnectType::SHORT3: 
      case OmniConnectType::SHORT4: {GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->IntArray); ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_FLATTEN_MACRO(VtIntArray, unsigned short); break; }
      case OmniConnectType::UINT2:
      case OmniConnectType::UINT3: 
      case OmniConnectType::UINT4: {GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->UIntArray); ASSIGN_ARRAY_TO_PRIMVAR_FLATTEN_MACRO(VtUIntArray); break; }
      case OmniConnectType::LONG2:
      case OmniConnectType::LONG3: 
      case OmniConnectType::LONG4: {GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->Int64Array); ASSIGN_ARRAY_TO_PRIMVAR_FLATTEN_MACRO(VtInt64Array); break; }
      case OmniConnectType::ULONG2:
      case OmniConnectType::ULONG3: 
      case OmniConnectType::ULONG4: {GET_PRIMVAR_BY_NAME_MACRO(SdfValueTypeNames->UInt64Array); ASSIGN_ARRAY_TO_PRIMVAR_FLATTEN_MACRO(VtUInt64Array); break; }

      default: {OmniConnectErrorMacro("Generic Array update does not support type: " << genericArrays[i].DataType) break; }
    };
  }
}

void OmniConnectInternals::DeleteGenericArrays(UsdGeomPrimvarsAPI& actorPrimvarsApi, UsdGeomPrimvarsAPI& clipPrimvarsApi, UsdGeomPrimvarsAPI& topPrimvarsApi, 
  OmniConnectGenericArray* genericArrays, size_t numGenericArrays, double)
{
  for (int i = 0; i < numGenericArrays; ++i)
  {
    //TfToken nameToken(genericArrays[i].Name);
    //UsdAttribute genericPrimvar = actorGeom.GetPrimvar(nameToken);
    //if (genericPrimvar)
    //  genericPrimvar.ClearAtTime(timeCode);

    TfToken attribToken(genericArrays[i].Name);
    actorPrimvarsApi.RemovePrimvar(attribToken);
    clipPrimvarsApi.RemovePrimvar(attribToken);
    topPrimvarsApi.RemovePrimvar(attribToken);
  }
}


template<typename CacheType>
void OmniConnectInternals::CreateSceneGeom(UsdStageRefPtr& sceneStage, const OmniConnectGeomCache& geomCache)
{
  typedef typename UsdTypeFromCache<CacheType>::UsdPrimType UsdPrimType;
  typedef typename UsdTypeFromCache<CacheType>::AltUsdPrimType AltUsdPrimType;

  if(geomCache.UsesAltUsdPrimType)
    AltUsdPrimType::Define(sceneStage, geomCache.SdfGeomPath);
  else
    UsdPrimType::Define(sceneStage, geomCache.SdfGeomPath);
}

template<typename CacheType>
void OmniConnectInternals::UpdateSceneGeom(UsdStageRefPtr& sceneStage, const CacheType& geomCache, typename CacheType::GeomDataType& geomData)
{
}

template<>
void OmniConnectInternals::UpdateSceneGeom<OmniConnectInstancerCache>(UsdStageRefPtr& sceneStage, const OmniConnectInstancerCache& geomCache, OmniConnectInstancerData& omniInstancerData)
{
  // The instancer uses an external shape as prototype. Find actors with name starting with the source shape identifier
  // and the prototype of the geometry of geomCache to this actor
  bool hasSourceShape = omniInstancerData.ShapeSourceNameSubstr;
  SdfPath protoActPath;

  if(hasSourceShape)
  {
    // Find the actor
    UsdPrim actScopePrim = sceneStage->GetPrimAtPath(this->SdfActScopeName);
    UsdPrimSiblingRange children = actScopePrim.GetAllChildren();
    for (UsdPrim child : children)
    {
      SdfPath childPath = child.GetPath();
      if(strstr(childPath.GetString().c_str(), omniInstancerData.ShapeSourceNameSubstr))
      {
        protoActPath = childPath;
      }
    }

    // If no actor to connect to, fall back to original shape.
    if(protoActPath.IsEmpty())
    {
      hasSourceShape = false;
      OmniConnectErrorMacro("UsdGeomPointInstancer with custom source prototype shape can find no prototype actor with name: " << omniInstancerData.ShapeSourceNameSubstr << ". If this name refers to an invisible actor, make sure the actor has been made visible at least once before setting it to invisible.");
    }
  }

  UsdGeomPointInstancer instancer = UsdGeomPointInstancer::Get(sceneStage, geomCache.SdfGeomPath);
  if(instancer)
  {
    UsdRelationship protoRel = instancer.GetPrototypesRel();
    if(hasSourceShape)
    {
      SdfPath protoShapePath = geomCache.SdfProtoPath.AppendPath(geomCache.RelExternalSourcePath);

      // Create a prototype prim which forces visibility to true (in the usual case where the prototype prim is invisible),
      // and no material bindings.
      UsdGeomXform protoSourcePrim = UsdGeomXform::Define(sceneStage, protoShapePath);

      UsdReferences protoRef = protoSourcePrim.GetPrim().GetReferences();
      protoRef.ClearReferences();
      protoRef.AddInternalReference(protoActPath);

      protoSourcePrim.MakeVisible();

      UsdPrimSiblingRange protoGeomPrims = protoSourcePrim.GetPrim().GetAllChildren();
      for (UsdPrim protoGeomPrim : protoGeomPrims)
      {
        UsdRelationship materialRel = protoGeomPrim.GetRelationship(OmniConnectTokens->materialBinding);
        if(materialRel)
          materialRel.SetTargets(SdfPathVector()); // Material of the prototype should be overridden to be None, use the material from the instancer instead (could interfere)
      }

      // Set the rel to the previously created prototype shape
      if(!protoRel)
        protoRel = instancer.CreatePrototypesRel();
      protoRel.SetTargets({protoShapePath});
    }
    else if(protoRel)
      protoRel.ClearTargets(true);
  }
}

void OmniConnectInternals::RemoveSceneGeom(UsdStageRefPtr& sceneStage, const SdfPath& geomPath)
{
  sceneStage->RemovePrim(geomPath);
}

template<typename CacheType>
void OmniConnectInternals::UpdateGeom(OmniConnectActorCache* actorCache, double animTimeStep, typename CacheType::GeomDataType& geomData, size_t geomId, size_t materialId,
  OmniConnectGenericArray* updatedGenericArrays, size_t numUga, OmniConnectGenericArray* deletedGenericArrays, size_t numDga)
{
  auto geomCachePair = actorCache->GetGeomCache<CacheType>(geomId);

  CacheType& geomCache = geomCachePair.second;
  bool newGeomCache = geomCachePair.first;

  // In case of reopening an existing scene, make sure that the geom cache's clip actives and paths 
  // are synchronized with any existing files before adding to them
  if(newGeomCache && !Settings.CreateNewOmniSession)
  {
    RestoreClipActivesAndPaths(*actorCache, geomCache);
  }

  bool newGeom = this->UpdateActorGeom(*actorCache, geomCache, geomData, animTimeStep,
    updatedGenericArrays, numUga, deletedGenericArrays, numDga);

  if (newGeom)
  {
    // Under the assumption that timesteps are generally added in sequence starting from 0, 
    // the 0 timestep is added to set invisibility for all timesteps up until animTimeStep
    UsdGeomImageable geomIm = UsdGeomImageable::Get(actorCache->Stage, geomCache.SdfGeomPath);
    assert(geomIm);
    geomIm.CreateVisibilityAttr().Set(UsdGeomTokens->invisible, UsdTimeCode(0.0));

    // Create the geometry in the scene stage too
    this->CreateSceneGeom<CacheType>(this->SceneStage, geomCache);
  }

  this->UpdateSceneGeom<CacheType>(this->SceneStage, geomCache, geomData);

  if(!geomCache.HasPrivateMaterial)
    this->SetMaterialBinding<CacheType>(*actorCache, geomId, materialId);
}

template<typename CacheType>
bool OmniConnectInternals::RemoveGeomAtTime(OmniConnectActorCache* actorCache, double animTimeStep, size_t geomId)
{
  std::map<size_t, CacheType>& geomCaches = actorCache->GetGeomCaches<CacheType>();
  
  auto geomCacheIt = geomCaches.find(geomId);
  if (geomCacheIt == geomCaches.end())
    return true; // Actor is not there anymore, so the geom doesn't exist either.

  CacheType& geomCache = geomCacheIt->second;

  bool geomDeleted = this->RemoveActorGeomAtTime<CacheType>(*actorCache, geomCache, animTimeStep);
  if (geomDeleted)
  {
    this->RemoveSceneGeom(this->SceneStage, geomCache.SdfGeomPath);
    geomCaches.erase(geomCacheIt);
  }
  return geomDeleted;
}

template<typename CacheType>
void OmniConnectInternals::RemoveGeom(OmniConnectActorCache* actorCache, size_t geomId)
{
  std::map<size_t, CacheType>& geomCaches = actorCache->GetGeomCaches<CacheType>();

  auto geomCacheIt = geomCaches.find(geomId);
  if (geomCacheIt == geomCaches.end())
    return;

  CacheType& geomCache = geomCacheIt->second;

  this->RemoveActorGeom<CacheType>(*actorCache, geomCache);
  this->RemoveSceneGeom(this->SceneStage, geomCache.SdfGeomPath);

  geomCaches.erase(geomCacheIt);
}

template<typename CacheType, typename PrimType>
void OmniConnectInternals::GetActorGeomPrimsAndStages(OmniConnectActorCache& actorCache, CacheType& geomCache, double animTimeStep,
  PrimType& actorGeom, PrimType& clipGeom, PrimType& topGeom, UsdStageRefPtr& geomClipStage, UsdStageRefPtr& geomTopologyStage, bool& newGeom, bool& newTimeStep)
{
  // Create a geom for the group.
  const SdfPath& geomPath = geomCache.SdfGeomPath;
  UsdPrim geomPrim;
  actorGeom = PrimType::Get(actorCache.Stage, geomPath);
  newGeom = !actorGeom;
  if (newGeom)
  {
    if(actorCache.Stage->GetPrimAtPath(geomPath))
    { // See DefineOnEmptyPrim; existing path but with different primtype implies a total removal of existing prim.
      // Clips are bluntly reset, any existing clip files disregarded.
      // Make sure to signal the geom type change to connector externals to reset any parent stage properties such as derived clip values.
      geomCache.ClipAssetPaths.clear();
      geomCache.ClipActives.clear();
      actorCache.GeomTypeChanged = true;
    }
    actorGeom = DefineOnEmptyPrim<PrimType>(actorCache.Stage, geomPath);
  }
  geomPrim = actorGeom.GetPrim();

  // Get the clip/topology files/paths
  geomCache.ResetTopologyFile(actorCache, this->LiveInterface.GetLiveExtension());
  geomCache.ResetTimedGeomClipFile(animTimeStep, this->LiveInterface.GetLiveExtension());
  std::string& geomClipFile = geomCache.TimedSceneRelGeomClipFile;
  std::string& geomTopologyFile = geomCache.SceneRelGeomTopologyFile;
  std::string& geomClipStagePath = geomCache.TimedGeomClipStagePath;
  std::string& geomTopologyStagePath = geomCache.GeomTopologyStagePath;

  // Create/Open the geometry and topology stage for this timestep.
  OpenClipAndTopologyStage(geomClipStagePath.c_str(), geomTopologyStagePath.c_str(),
    geomClipStage, geomTopologyStage);

  // Populate new timestep stage and add to the value clip
  clipGeom = PrimType::Get(geomClipStage, geomPath);
  topGeom = PrimType::Get(geomTopologyStage, geomPath);
  newTimeStep = !clipGeom;
  if (newTimeStep)
  {
    UsdClipsAPI clipsApi(geomPrim);
    bool saveTopGeom = false;
    if (!topGeom) // Either because of newGeom, or because of live stage
    {
      topGeom = DefineOnEmptyPrim<PrimType>(geomTopologyStage, geomPath);
      saveTopGeom = true;
    }

    if (newGeom)
    {
      clipsApi.SetClipPrimPath(geomCache.GeomPath);
      clipsApi.SetClipManifestAssetPath(SdfAssetPath(geomTopologyFile));

      InitializeGeom(actorGeom, actorCache.Stage, geomCache, true);
      InitializeGeom(topGeom, geomTopologyStage, geomCache, false);

      saveTopGeom = true;

#ifdef TOPOLOGY_AS_SUBLAYER
      //Add topology as sublayer
      actorCache.Stage->GetRootLayer()->InsertSubLayerPath(geomTopologyFile);
#else
      geomPrim.GetReferences().AddReference(geomTopologyFile, geomPath);
#endif
    }
    assert(topGeom);
    if(UsdSaveEnabled && saveTopGeom)
      geomTopologyStage->Save();

#ifdef FORCE_OMNI_CLIP_UPDATES_WITH_SUBLAYER      
    actorCache.Stage->GetRootLayer()->InsertSubLayerPath(geomClipFile);
#endif

    VtArray<SdfAssetPath>& assetPaths = geomCache.ClipAssetPaths;
    LiveInterface.FilterAssetPaths(assetPaths, geomClipFile, geomClipStage);
    size_t assetPathIdx = assetPaths.size();
    assetPaths.push_back(SdfAssetPath(geomClipFile));
    clipsApi.SetClipAssetPaths(assetPaths);

    VtVec2dArray& clipActives = geomCache.ClipActives;
    bool modified = LiveInterface.ModifyClipActives(clipActives, animTimeStep, (double)assetPathIdx);
    if(!modified)
      clipActives.push_back(GfVec2d(animTimeStep, (double)assetPathIdx));
    clipsApi.SetClipActive(clipActives);

    //Define the standard geom prim.
    clipGeom = DefineOnEmptyPrim<PrimType>(geomClipStage, geomPath);

    InitializeGeom(clipGeom, geomClipStage, geomCache, false);
  }
}

void OmniConnectInternals::InitializeGeom(UsdGeomMesh& mesh, UsdStageRefPtr& geomStage, OmniConnectMeshCache& meshCache, bool setDefaultValues)
{
  UsdGeomPrimvarsAPI primvarApi(mesh);

  mesh.CreatePointsAttr();
  mesh.CreateExtentAttr();
  mesh.CreateFaceVertexIndicesAttr();
  mesh.CreateFaceVertexCountsAttr();
  mesh.CreateNormalsAttr();
  mesh.CreateDisplayColorPrimvar();
  mesh.CreateDisplayOpacityPrimvar();
  primvarApi.CreatePrimvar(TEX_PRIMVAR_NAME, SdfValueTypeNames->TexCoord2fArray);
#if defined(USE_MDL_MATERIALS) && USE_CUSTOM_MDL
  primvarApi.CreatePrimvar(OmniConnectTokens->st1, SdfValueTypeNames->TexCoord2fArray);
  primvarApi.CreatePrimvar(OmniConnectTokens->st2, SdfValueTypeNames->TexCoord2fArray);
#endif

  if (setDefaultValues)
  {
    mesh.CreateDoubleSidedAttr(VtValue(true));
    mesh.CreateSubdivisionSchemeAttr().Set(UsdGeomTokens->none);
  }
}

void OmniConnectInternals::InitializeGeom(UsdGeomPointInstancer& points, UsdStageRefPtr& geomStage, OmniConnectInstancerCache& instancerCache, bool setDefaultValues)
{
  UsdGeomPrimvarsAPI primvarApi(points);

  points.CreatePositionsAttr();
  points.CreateExtentAttr();
  points.CreateIdsAttr();
  points.CreateOrientationsAttr();
  points.CreateScalesAttr();
  primvarApi.CreatePrimvar(OmniConnectTokens->displayColor, SdfValueTypeNames->Color3fArray);
  primvarApi.CreatePrimvar(OmniConnectTokens->displayOpacity, SdfValueTypeNames->FloatArray);
  primvarApi.CreatePrimvar(TEX_PRIMVAR_NAME, SdfValueTypeNames->TexCoord2fArray);
#if defined(USE_MDL_MATERIALS) && USE_CUSTOM_MDL
  primvarApi.CreatePrimvar(OmniConnectTokens->st1, SdfValueTypeNames->TexCoord2fArray);
  primvarApi.CreatePrimvar(OmniConnectTokens->st2, SdfValueTypeNames->TexCoord2fArray);
#endif
  points.CreateProtoIndicesAttr();
  points.CreateVelocitiesAttr();
  points.CreateAngularVelocitiesAttr();
  points.CreateInvisibleIdsAttr(); 
}

void OmniConnectInternals::InitializeGeom(UsdGeomPoints& points, UsdStageRefPtr& geomStage, OmniConnectInstancerCache& instancerCache, bool setDefaultValues)
{
  UsdGeomPrimvarsAPI primvarApi(points);

  points.CreatePointsAttr();
  points.CreateExtentAttr();
  points.CreateIdsAttr();
  points.CreateNormalsAttr();
  points.CreateWidthsAttr();
  points.CreateDisplayColorPrimvar();
  points.CreateDisplayOpacityPrimvar();
  primvarApi.CreatePrimvar(TEX_PRIMVAR_NAME, SdfValueTypeNames->TexCoord2fArray);
#if defined(USE_MDL_MATERIALS) && USE_CUSTOM_POINT_SHADER
  primvarApi.CreatePrimvar(OmniConnectTokens->st, SdfValueTypeNames->TexCoord2fArray);
#endif
#if defined(USE_MDL_MATERIALS) && (USE_CUSTOM_MDL || USE_CUSTOM_POINT_SHADER)
  primvarApi.CreatePrimvar(OmniConnectTokens->st1, SdfValueTypeNames->TexCoord2fArray);
  primvarApi.CreatePrimvar(OmniConnectTokens->st2, SdfValueTypeNames->TexCoord2fArray);
#endif
}

void OmniConnectInternals::InitializeGeom(UsdGeomBasisCurves& curves, UsdStageRefPtr & geomStage, OmniConnectCurveCache & curveCache, bool setDefaultValues)
{
  UsdGeomPrimvarsAPI primvarApi(curves);

  curves.CreatePointsAttr();
  curves.CreateExtentAttr();
  curves.CreateCurveVertexCountsAttr();
  curves.CreateNormalsAttr();
  curves.CreateWidthsAttr();
  curves.CreateDisplayColorPrimvar();
  curves.CreateDisplayOpacityPrimvar();
  primvarApi.CreatePrimvar(TEX_PRIMVAR_NAME, SdfValueTypeNames->TexCoord2fArray);
#if defined(USE_MDL_MATERIALS) && USE_CUSTOM_MDL
  primvarApi.CreatePrimvar(OmniConnectTokens->st1, SdfValueTypeNames->TexCoord2fArray);
  primvarApi.CreatePrimvar(OmniConnectTokens->st2, SdfValueTypeNames->TexCoord2fArray);
#endif

  if (setDefaultValues)
  {
    curves.CreateDoubleSidedAttr(VtValue(true));
    curves.GetTypeAttr().Set(UsdGeomTokens->linear);
  }
}

void OmniConnectInternals::InitializeGeom(UsdVolVolume& volume, UsdStageRefPtr& geomStage, OmniConnectVolumeCache& volumeCache, bool setDefaultValues)
{
  volume.CreateExtentAttr();

  //using FieldInfoType = std::pair<const SdfPath&, const TfToken&>;
  //FieldInfoType fieldInfo[2] =
  //{ 
  //  FieldInfoType(volumeCache.OvdbDensityFieldPath, OmniConnectTokens->density),
  //  FieldInfoType(volumeCache.OvdbDiffuseFieldPath, OmniConnectTokens->diffuse)
  //};
  //
  //for (int i = 0; i < 1; ++i) // The update stage now takes care of all fields
  //{
  //  InitializeVolumeField(geomStage, fieldInfo[i].first, fieldInfo[i].second, setDefaultValues);
  //  
  //  if(setDefaultValues)
  //    volume.CreateFieldRelationship(fieldInfo[i].second, fieldInfo[i].first);
  //}
}

UsdVolOpenVDBAsset OmniConnectInternals::InitializeVolumeField(UsdStageRefPtr& stage, const SdfPath& path, const TfToken& token, bool setDefaultValues) const
{
  UsdVolOpenVDBAsset ovdbField = UsdVolOpenVDBAsset::Define(stage, path);

  ovdbField.CreateFilePathAttr();

  if (setDefaultValues)
  {
    ovdbField.CreateFieldNameAttr(VtValue(token));
  }

  return ovdbField;
}

UsdVolOpenVDBAsset OmniConnectInternals::CreateOrRemoveOpenVDBAsset(UsdStageRefPtr& stage, const SdfPath& primPath, const TfToken& token, bool create, bool setDefaultValues) const
  {
    UsdVolOpenVDBAsset ovdbAsset = UsdVolOpenVDBAsset::Get(stage, primPath);
    if (create)
    {
      if(!ovdbAsset)
        ovdbAsset = InitializeVolumeField(stage, primPath, token, setDefaultValues);
    }
    else
    {
      if(ovdbAsset)
        stage->RemovePrim(primPath);
    }
    return ovdbAsset;
  }

void OmniConnectInternals::InitializeGeom(UsdGeomMesh& volMesh, UsdStageRefPtr& geomStage, OmniConnectVolumeCache& volumeCache, bool setDefaultValues)
{
  // For mesh volumes, the material is intrinsically bound to the geometry, since the data is set as a material texture parameter.
  // Therefore a custom material is created in the mesh scope instead of using the generic material binding through SetMaterialBinding().
  UsdShadeMaterial volMat = UsdShadeMaterial::Define(geomStage, volumeCache.MeshVolMatPath);
  UsdShadeShader volShad = UsdShadeShader::Define(geomStage, volumeCache.MeshVolShadPath);

  // Create possibly timevarying attributes
  volMesh.CreatePointsAttr();
  volMesh.CreateExtentAttr();

#ifdef USE_MDL_MATERIALS
  volShad.CreateInput(OmniConnectTokens->volume_density_texture, SdfValueTypeNames->Asset);
#endif

  if (setDefaultValues)
  {
    UsdGeomPrimvarsAPI primvarApi(volMesh);

    // These attributes are not timevarying
    volMesh.CreateFaceVertexIndicesAttr();
    volMesh.CreateFaceVertexCountsAttr();
    volMesh.CreateNormalsAttr();
    primvarApi.CreatePrimvar(OmniConnectTokens->st, SdfValueTypeNames->TexCoord2fArray);
    primvarApi.CreatePrimvar(OmniConnectTokens->isVolume, SdfValueTypeNames->Bool);

    // Set default values
    volMesh.SetNormalsInterpolation(UsdGeomTokens->faceVarying);
    primvarApi.GetPrimvar(OmniConnectTokens->st).SetInterpolation(UsdGeomTokens->faceVarying);

    primvarApi.GetPrimvar(OmniConnectTokens->isVolume).Set(VtValue(true));

    volMesh.CreateSubdivisionSchemeAttr().Set(UsdGeomTokens->none);
    volMesh.GetPrim().ApplyAPI<UsdShadeMaterialBindingAPI>();
    UsdShadeMaterialBindingAPI(volMesh.GetPrim()).Bind(volMat);

    VtArray<int> faceVertexCounts({ 4, 4, 4, 4, 4, 4 });
    volMesh.GetFaceVertexCountsAttr().Set(faceVertexCounts);
    VtArray<int> faceVertexIndices({ 0, 1, 3, 2, 0, 4, 5, 1, 1, 5, 6, 3, 2, 3, 6, 7, 0, 2, 7, 4, 4, 7, 6, 5 });
    volMesh.GetFaceVertexIndicesAttr().Set(faceVertexIndices);
    VtVec2fArray texCoords({
      GfVec2f(1, 0), GfVec2f(0, 0), GfVec2f(0, 1), GfVec2f(1, 1), 
      GfVec2f(1, 0), GfVec2f(1, 1), GfVec2f(0, 1), GfVec2f(0, 0), 
      GfVec2f(1, 0), GfVec2f(0, 0), GfVec2f(0, 1), GfVec2f(1, 1), 
      GfVec2f(1, 0), GfVec2f(0, 0), GfVec2f(0, 1), GfVec2f(1, 1), 
      GfVec2f(1, 0), GfVec2f(1, 1), GfVec2f(0, 1), GfVec2f(0, 0), 
      GfVec2f(1, 0), GfVec2f(1, 1), GfVec2f(0, 1), GfVec2f(0, 0) 
    });
    primvarApi.GetPrimvar(OmniConnectTokens->st).Set(texCoords);
    VtVec3fArray normals({
      GfVec3f(0, -1, 0), GfVec3f(0, -1, 0), GfVec3f(0, -1, 0), GfVec3f(0, -1, 0),
      GfVec3f(0, 0, -1), GfVec3f(0, 0, -1), GfVec3f(0, 0, -1), GfVec3f(0, 0, -1),
      GfVec3f(1, 0, 0),  GfVec3f(1, 0, 0), GfVec3f(1, 0, 0),   GfVec3f(1, 0, 0),
      GfVec3f(0, 0, 1),  GfVec3f(0, 0, 1), GfVec3f(0, 0, 1),   GfVec3f(0, 0, 1),
      GfVec3f(-1, 0, 0), GfVec3f(-1, 0, 0), GfVec3f(-1, 0, 0), GfVec3f(-1, 0, 0),
      GfVec3f(0, 1, 0),  GfVec3f(0, 1, 0), GfVec3f(0, 1, 0),   GfVec3f(0, 1, 0)
    });
    volMesh.GetNormalsAttr().Set(normals);

#ifdef USE_MDL_MATERIALS
    UsdShadeOutput shadOut = CreateUsdMdlMeshVolumeShader(volShad, MdlNames);

    volMat.CreateSurfaceOutput(OmniConnectTokens->mdl).ConnectToSource(shadOut);
    volMat.CreateVolumeOutput(OmniConnectTokens->mdl).ConnectToSource(shadOut);
    volMat.CreateDisplacementOutput(OmniConnectTokens->mdl).ConnectToSource(shadOut);
#endif
  }
}

namespace
{
  
  template<typename UsdGeomType, typename MemFncClassType>
  void SyncTimeVaryingAttribute(const UsdAttribute& actAttrib, const UsdAttribute& clipAttrib, UsdGeomType& topGeom, 
    bool timeVarying, const UsdTimeCode& timeCode, const TfToken& primvarName,
    UsdAttribute (MemFncClassType::*topAttribCreateFun)(VtValue const&, bool) const)
  {
    if (timeVarying)
      (topGeom.*topAttribCreateFun)(VtValue(), false);
    else
      topGeom.GetPrim().RemoveProperty(primvarName);

    if (timeVarying)
      actAttrib.Clear();
    else
      clipAttrib.ClearAtTime(timeCode);
  }

  void SyncTimeVaryingInput(UsdShadeShader& actShader, UsdShadeShader& clipShader, UsdShadeShader& topShader, 
    bool timeVarying, const UsdTimeCode& timeCode, const TfToken& inputName, const char* inputNameStr, const SdfValueTypeName& primvarType)
  {
    UsdAttribute actAttrib = actShader.GetInput(inputName).GetAttr();
    UsdAttribute clipAttrib = clipShader.GetInput(inputName).GetAttr();

    if (timeVarying)
      topShader.CreateInput(inputName, primvarType);
    else
      topShader.GetPrim().RemoveProperty(TfToken(inputNameStr));

    if (timeVarying)
      actAttrib.Clear();
    else
      clipAttrib.ClearAtTime(timeCode);
  }

  void SyncTimeVaryingPrimvar(const UsdGeomPrimvar& actPrimvar, const UsdGeomPrimvar& clipPrimvar, UsdGeomPrimvarsAPI& topPrimvarsApi, 
    bool timeVarying, const UsdTimeCode& timeCode, const TfToken& primvarName, const SdfValueTypeName& primvarType)
  {
    if (timeVarying)
      topPrimvarsApi.CreatePrimvar(primvarName, primvarType);
    else
      topPrimvarsApi.RemovePrimvar(primvarName);

    if (timeVarying)
      actPrimvar.GetAttr().Clear();
    else
      clipPrimvar.GetAttr().ClearAtTime(timeCode);
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomPoints(UsdGeomType& actGeom, UsdGeomType& clipGeom, UsdGeomType& topGeom, const GeomDataType& omniGeomData, uint64_t numPrims,
    OmniConnectUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
  {
    using DMI = typename GeomDataType::DataMemberId;
    // Fill geom prim and geometry layer with data.
    bool performsUpdate = updateEval.PerformsUpdate(DMI::POINTS);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::POINTS);

    // SyncTimeVaryingAttribute() 
    if (timeVaryingUpdate)
      UsdGeomCreatePointsAttribute(topGeom);
    else
      UsdGeomRemovePointsAttribute(topGeom);

    if (timeVaryingUpdate)
      UsdGeomGetPointsAttribute(actGeom).Clear();
    else
      UsdGeomGetPointsAttribute(clipGeom).ClearAtTime(timeEval.TimeCode);
    //~SyncTimeVaryingAttribute()

    SyncTimeVaryingAttribute(actGeom.GetExtentAttr(), clipGeom.GetExtentAttr(), topGeom, 
      timeVaryingUpdate, timeEval.TimeCode, OmniConnectTokens->extent, &UsdGeomType::CreateExtentAttr);

    if (performsUpdate)
    {
      UsdGeomType& outGeom = timeVaryingUpdate ? clipGeom : actGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::POINTS);

      // Points
      //outGeom.GetPrim().RemoveProperty(OmniConnectTokens->points);
      UsdAttribute pointsAttr = UsdGeomGetPointsAttribute(outGeom);
      assert(pointsAttr);

      void* arrayData = omniGeomData.Points;
      size_t arrayNumElements = omniGeomData.NumPoints;
      UsdAttribute arrayPrimvar = pointsAttr;
      VtVec3fArray usdVerts;
      bool setPrimvar = true;
      switch (omniGeomData.PointsType)
      {
      case OmniConnectType::FLOAT3: {ASSIGN_CUSTOM_ARRAY_TO_PRIMVAR_MACRO(VtVec3fArray, usdVerts); break; }
      case OmniConnectType::DOUBLE3: {ASSIGN_CUSTOM_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtVec3fArray, GfVec3d, usdVerts); break; }
      default: { pointsAttr.ClearAtTime(timeCode); OmniConnectErrorMacro("UsdGeom PointsAttr does not support type: " << omniGeomData.PointsType) break; }
      }

      // Usd requires extent.
      GfRange3f extent;
      for (const auto& pt : usdVerts) {
        extent.UnionWith(pt);
      }
      VtVec3fArray extentArray(2);
      extentArray[0] = extent.GetMin();
      extentArray[1] = extent.GetMax();

      // Extent remains part of the main usd. This also allows us to implicitly cache the array of timecodes.
      outGeom.GetExtentAttr().Set(extentArray, timeCode);
    }
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomNormals(UsdGeomType& actGeom, UsdGeomType& clipGeom, UsdGeomType& topGeom, const GeomDataType& omniGeomData, uint64_t numPrims,
    OmniConnectUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
  {
    using DMI = typename GeomDataType::DataMemberId;
    bool performsUpdate = updateEval.PerformsUpdate(DMI::NORMALS);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::NORMALS);

    SyncTimeVaryingAttribute(actGeom.GetNormalsAttr(), clipGeom.GetNormalsAttr(), topGeom,
      timeVaryingUpdate, timeEval.TimeCode, OmniConnectTokens->normals, &UsdGeomType::CreateNormalsAttr);
      
    if (performsUpdate)
    {
      UsdGeomType& outGeom = timeVaryingUpdate ? clipGeom : actGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::NORMALS);

      //outGeom.GetPrim().RemoveProperty(OmniConnectTokens->normals);
      UsdAttribute normalsAttr = outGeom.GetNormalsAttr();
      assert(normalsAttr);

      if (omniGeomData.Normals != nullptr)
      {
        void* arrayData = omniGeomData.Normals;
        size_t arrayNumElements = omniGeomData.PerPrimNormals ? numPrims : omniGeomData.NumPoints;
        UsdAttribute arrayPrimvar = normalsAttr;
        bool setPrimvar = true;
        switch (omniGeomData.NormalsType)
        {
        case OmniConnectType::FLOAT3: {ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec3fArray); break; }
        case OmniConnectType::DOUBLE3: {ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtVec3fArray, GfVec3d); break; }
        default: { normalsAttr.ClearAtTime(timeCode); OmniConnectErrorMacro("UsdGeom NormalsAttr does not support type: " << omniGeomData.NormalsType) break; }
        }

        // Per face or per-vertex interpolation. This will break timesteps that have been written before.
        TfToken normalInterpolation = omniGeomData.PerPrimNormals ? UsdGeomTokens->uniform : UsdGeomTokens->vertex;
        actGeom.SetNormalsInterpolation(normalInterpolation);
      }
      else
      {
        normalsAttr.Set(SdfValueBlock(), timeCode);
      }
    }
  }

  template<typename GeomDataType>
  void UpdateUsdGeomTexCoords(UsdGeomPrimvarsAPI& actorPrimvarsApi, UsdGeomPrimvarsAPI& clipPrimvarsApi, UsdGeomPrimvarsAPI& topPrimvarsApi, 
    const GeomDataType& omniGeomData, uint64_t numPrims, OmniConnectUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
  {
    using DMI = typename GeomDataType::DataMemberId;
    bool performsUpdate = updateEval.PerformsUpdate(DMI::TEXCOORDS);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::TEXCOORDS);

    UsdGeomPrimvar actPrimvar = actorPrimvarsApi.GetPrimvar(TEX_PRIMVAR_NAME);
    UsdGeomPrimvar clipPrimvar = clipPrimvarsApi.GetPrimvar(TEX_PRIMVAR_NAME);

    SyncTimeVaryingPrimvar(actPrimvar, clipPrimvar, topPrimvarsApi, timeVaryingUpdate, timeEval.TimeCode,
      TEX_PRIMVAR_NAME, SdfValueTypeNames->TexCoord2fArray);

    if (performsUpdate)
    {
      UsdTimeCode timeCode = timeEval.Eval(DMI::TEXCOORDS);

      UsdAttribute texcoordPrimvar = timeVaryingUpdate ? clipPrimvar : actPrimvar;
      assert(texcoordPrimvar);

      if (omniGeomData.TexCoords != nullptr)
      {
        void* arrayData = omniGeomData.TexCoords;
        size_t arrayNumElements = omniGeomData.PerPrimTexCoords ? numPrims : omniGeomData.NumPoints;
        UsdAttribute arrayPrimvar = texcoordPrimvar;
        bool setPrimvar = true;
        switch (omniGeomData.TexCoordsType)
        {
        case OmniConnectType::FLOAT2: {ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec2fArray); break; }
        case OmniConnectType::DOUBLE2: {ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtVec2fArray, GfVec2d); break; }
        default: { texcoordPrimvar.ClearAtTime(timeCode); OmniConnectErrorMacro("UsdGeom TexCoordsPrimvar does not support type: " << omniGeomData.TexCoordsType) break; }
        }

        // Per face or per-vertex interpolation. This will break timesteps that have been written before.
        TfToken texcoordInterpolation = omniGeomData.PerPrimTexCoords ? UsdGeomTokens->uniform : UsdGeomTokens->vertex;
        actPrimvar.SetInterpolation(texcoordInterpolation);
      }
      else
      {
        texcoordPrimvar.Set(SdfValueBlock(), timeCode);
      }
    }
  }

  template<typename GeomDataType>
  void UpdateUsdGeomColors(UsdGeomPrimvarsAPI& actorPrimvarsApi, UsdGeomPrimvarsAPI& clipPrimvarsApi, UsdGeomPrimvarsAPI& topPrimvarsApi, 
    const GeomDataType& omniGeomData, uint64_t numPrims, OmniConnectUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval
#if defined(USE_MDL_MATERIALS) && USE_CUSTOM_POINT_SHADER
    , bool isPointsGeom = false
#endif
    )
  {
    using DMI = typename GeomDataType::DataMemberId;
    bool performsUpdate = updateEval.PerformsUpdate(DMI::COLORS);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::COLORS);

    UsdGeomPrimvar actPrimvarDispC = actorPrimvarsApi.GetPrimvar(OmniConnectTokens->displayColor);
    UsdGeomPrimvar clipPrimvarDispC = clipPrimvarsApi.GetPrimvar(OmniConnectTokens->displayColor);
    UsdGeomPrimvar actPrimvarDispO = actorPrimvarsApi.GetPrimvar(OmniConnectTokens->displayOpacity);
    UsdGeomPrimvar clipPrimvarDispO = clipPrimvarsApi.GetPrimvar(OmniConnectTokens->displayOpacity);

#if defined(USE_MDL_MATERIALS) && (USE_CUSTOM_MDL || USE_CUSTOM_POINT_SHADER)
    UsdGeomPrimvar actPrimvarSt1, clipPrimvarSt1, actPrimvarSt2, clipPrimvarSt2;
#if USE_CUSTOM_POINT_SHADER
    if(isPointsGeom)
#endif
    {
      actPrimvarSt1 = actorPrimvarsApi.GetPrimvar(OmniConnectTokens->st1);
      clipPrimvarSt1 = clipPrimvarsApi.GetPrimvar(OmniConnectTokens->st1);
      actPrimvarSt2 = actorPrimvarsApi.GetPrimvar(OmniConnectTokens->st2);
      clipPrimvarSt2 = clipPrimvarsApi.GetPrimvar(OmniConnectTokens->st2);
    }
#endif

    SyncTimeVaryingPrimvar(actPrimvarDispC, clipPrimvarDispC, topPrimvarsApi, timeVaryingUpdate, timeEval.TimeCode,
      OmniConnectTokens->displayColor, SdfValueTypeNames->TexCoord2fArray);
    SyncTimeVaryingPrimvar(actPrimvarDispO, clipPrimvarDispO, topPrimvarsApi, timeVaryingUpdate, timeEval.TimeCode,
      OmniConnectTokens->displayOpacity, SdfValueTypeNames->TexCoord2fArray);
#if defined(USE_MDL_MATERIALS) && (USE_CUSTOM_MDL || USE_CUSTOM_POINT_SHADER)
#if USE_CUSTOM_POINT_SHADER
    if(isPointsGeom)
#endif
    {
      SyncTimeVaryingPrimvar(actPrimvarSt1, clipPrimvarSt1, topPrimvarsApi, timeVaryingUpdate, timeEval.TimeCode,
        OmniConnectTokens->st1, SdfValueTypeNames->TexCoord2fArray);
      SyncTimeVaryingPrimvar(actPrimvarSt2, clipPrimvarSt2, topPrimvarsApi, timeVaryingUpdate, timeEval.TimeCode,
        OmniConnectTokens->st2, SdfValueTypeNames->TexCoord2fArray);
    }
#endif

    if (performsUpdate)
    {
      UsdTimeCode timeCode = timeEval.Eval(DMI::COLORS);

      UsdGeomPrimvar colorPrimvar = timeVaryingUpdate ? clipPrimvarDispC : actPrimvarDispC;
      assert(colorPrimvar);
      UsdGeomPrimvar opacityPrimvar = timeVaryingUpdate ? clipPrimvarDispO : actPrimvarDispO;
      assert(opacityPrimvar);

#if defined(USE_MDL_MATERIALS) && (USE_CUSTOM_MDL || USE_CUSTOM_POINT_SHADER)
      UsdGeomPrimvar vc0Primvar, vc1Primvar;
#if USE_CUSTOM_POINT_SHADER
      if(isPointsGeom)
#endif
      {
        vc0Primvar = timeVaryingUpdate ? clipPrimvarSt1 : actPrimvarSt1;
        assert(vc0Primvar);
        vc1Primvar = timeVaryingUpdate ? clipPrimvarSt2 : actPrimvarSt2;
        assert(vc1Primvar);
      }
#endif

      const unsigned char* colors = omniGeomData.Colors;
      if (colors != nullptr)
      {
        uint64_t numColorEntries = omniGeomData.PerPrimColors ? numPrims : omniGeomData.NumPoints;
        VtArray<GfVec3f> usdVertColors(numColorEntries);
        VtFloatArray usdVertOpacities(numColorEntries);

        int ccM = omniGeomData.ColorComponents;
        const float* srgbTable = ocutils::SrgbToLinearTable();
        for (uint64_t i = 0; i < numColorEntries; ++i)
        {
          usdVertColors[i] = GfVec3f(srgbTable[colors[ccM * i]], srgbTable[colors[ccM * i + 1]], srgbTable[colors[ccM * i + 2]]);
          if(ccM > 3)
            usdVertOpacities[i] = (float)colors[ccM * i + 3]/255.0f;
        }

        colorPrimvar.Set(usdVertColors, timeCode);

        if(ccM > 3)
          opacityPrimvar.Set(usdVertOpacities, timeCode);
        else
          opacityPrimvar.GetAttr().Set(SdfValueBlock(), timeCode);

        // Per face or per-vertex interpolation. This will break timesteps that have been written before.
        TfToken colorInterpolation = omniGeomData.PerPrimColors ? UsdGeomTokens->uniform : UsdGeomTokens->vertex;
        actPrimvarDispC.SetInterpolation(colorInterpolation);
        actPrimvarDispO.SetInterpolation(colorInterpolation);

#if defined(USE_MDL_MATERIALS) && (USE_CUSTOM_MDL || USE_CUSTOM_POINT_SHADER)
#if USE_CUSTOM_POINT_SHADER
        if(isPointsGeom)
#endif
        {
          // Make sure the "st" array does not have values
          //UsdAttribute texcoordPrimvar = outPrimvars.GetPrimvar(OmniConnectTokens->st);
          //assert(texcoordPrimvar);

          //if (texcoordPrimvar.HasValue())
          //{
          //  texcoordPrimvar.ClearAtTime(timeCode);
          //}

          // Make sure the "st" array has values (hack to force vertexcolor_coordinate_index=1 for all geom types)
          {
            UsdGeomPrimvar actTexcoordPrimvar = actorPrimvarsApi.GetPrimvar(OmniConnectTokens->st);
            UsdGeomPrimvar texcoordPrimvar = timeVaryingUpdate ? clipPrimvarsApi.GetPrimvar(OmniConnectTokens->st) : actTexcoordPrimvar;
            assert(texcoordPrimvar);

            //if (!texcoordPrimvar.HasAuthoredValue()) // doesn't return false when timesamples are blocked, so just say colors <==> !texcoords
            {
              VtVec2fArray defaultTexCoords(omniGeomData.NumPoints);
              defaultTexCoords.assign(omniGeomData.NumPoints, GfVec2f(0.0f, 0.0f));

              texcoordPrimvar.Set(defaultTexCoords, timeCode);

              actTexcoordPrimvar.SetInterpolation(UsdGeomTokens->vertex);
            }
          }

          // Fill the vc arrays
          VtVec2fArray customVertexColors0(numColorEntries);
          VtVec2fArray customVertexColors1(numColorEntries);

          for (uint64_t i = 0; i < numColorEntries; ++i)
          {
            float alpha = (ccM == 4) ? (float(colors[ccM * i + 3]) / 255.0f) : 1.0f;
            customVertexColors0[i] = GfVec2f(usdVertColors[i][0], usdVertColors[i][1]);
            customVertexColors1[i] = GfVec2f(usdVertColors[i][2], alpha);
          }

          vc0Primvar.Set(customVertexColors0, timeCode);
          vc1Primvar.Set(customVertexColors1, timeCode);

          actPrimvarSt1.SetInterpolation(colorInterpolation);
          actPrimvarSt2.SetInterpolation(colorInterpolation);
        }
#endif
      }
      else
      {
        colorPrimvar.GetAttr().Set(SdfValueBlock(), timeCode);
        opacityPrimvar.GetAttr().Set(SdfValueBlock(), timeCode);
#if defined(USE_MDL_MATERIALS) && (USE_CUSTOM_MDL || USE_CUSTOM_POINT_SHADER)
#if USE_CUSTOM_POINT_SHADER
        if(isPointsGeom)
#endif
        {
          vc0Primvar.GetAttr().Set(SdfValueBlock(), timeCode);
          vc1Primvar.GetAttr().Set(SdfValueBlock(), timeCode);
        }
#endif
      }
    }
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomIndices(UsdGeomType& actGeom, UsdGeomType& clipGeom, UsdGeomType& topGeom, const GeomDataType& omniGeomData, uint64_t numPrims,
    OmniConnectUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
  {
    using DMI = typename GeomDataType::DataMemberId;
    bool performsUpdate = updateEval.PerformsUpdate(DMI::INDICES);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::INDICES);

    SyncTimeVaryingAttribute(actGeom.GetFaceVertexIndicesAttr(), clipGeom.GetFaceVertexIndicesAttr(), topGeom,
      timeVaryingUpdate, timeEval.TimeCode, OmniConnectTokens->faceVertexIndices, &UsdGeomType::CreateFaceVertexIndicesAttr);
    SyncTimeVaryingAttribute(actGeom.GetFaceVertexCountsAttr(), clipGeom.GetFaceVertexCountsAttr(), topGeom,
      timeVaryingUpdate, timeEval.TimeCode, OmniConnectTokens->faceVertexCounts, &UsdGeomType::CreateFaceVertexCountsAttr);

    if (performsUpdate)
    {
      UsdGeomType& outGeom = timeVaryingUpdate ? clipGeom : actGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::INDICES);

      int* indices = reinterpret_cast<int*>(omniGeomData.Indices);

      VtArray<int> usdVertexCounts(numPrims);
      int vertexCount = static_cast<int>(omniGeomData.NumIndices / numPrims);
      for (uint64_t i = 0; i < numPrims; ++i)
        usdVertexCounts[i] = vertexCount;//omniGeomData.FaceVertCounts[i];

      VtArray<int> usdIndices;
      usdIndices.assign(indices, indices + omniGeomData.NumIndices);

      // Face indices
      //outGeom.GetPrim().RemoveProperty(OmniConnectTokens->faceVertexIndices);
      UsdAttribute faceVertIndicesAttr = outGeom.GetFaceVertexIndicesAttr();
      assert(faceVertIndicesAttr);
      faceVertIndicesAttr.Set(usdIndices, timeCode);

      // Face Vertex counts
      //outGeom.GetPrim().RemoveProperty(OmniConnectTokens->faceVertexCounts);
      UsdAttribute faceVertCountsAttr = outGeom.GetFaceVertexCountsAttr();
      assert(faceVertCountsAttr);
      faceVertCountsAttr.Set(usdVertexCounts, timeCode);
    }
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomInstanceIds(UsdGeomType& actGeom, UsdGeomType& clipGeom, UsdGeomType& topGeom, const GeomDataType& omniGeomData, uint64_t numPrims,
    OmniConnectUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
  {
    using DMI = typename GeomDataType::DataMemberId;
    bool performsUpdate = updateEval.PerformsUpdate(DMI::INSTANCEIDS);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::INSTANCEIDS);

    SyncTimeVaryingAttribute(actGeom.GetIdsAttr(), clipGeom.GetIdsAttr(), topGeom,
      timeVaryingUpdate, timeEval.TimeCode, OmniConnectTokens->ids, &UsdGeomType::CreateIdsAttr);

    if (performsUpdate)
    {
      UsdGeomType& outGeom = timeVaryingUpdate ? clipGeom : actGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::INSTANCEIDS);

      //Instance ids
      UsdAttribute idsAttr = outGeom.GetIdsAttr();
      assert(idsAttr);
      if (omniGeomData.InstanceIds)
      {
        VtArray<int64_t> usdIds(omniGeomData.NumPoints);
        for (uint64_t i = 0; i < omniGeomData.NumPoints; ++i)
          usdIds[i] = omniGeomData.InstanceIds[i];

        idsAttr.Set(usdIds, timeCode);
      }
      else
      {
        idsAttr.Set(SdfValueBlock(), timeCode);
      }
    }
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomWidths(UsdGeomType& actGeom, UsdGeomType& clipGeom, UsdGeomType& topGeom, const GeomDataType& omniGeomData, uint64_t numPrims,
    OmniConnectUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
  {
    using DMI = typename GeomDataType::DataMemberId;
    bool performsUpdate = updateEval.PerformsUpdate(DMI::SCALES);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::SCALES);

    SyncTimeVaryingAttribute(actGeom.GetWidthsAttr(), clipGeom.GetWidthsAttr(), topGeom,
      timeVaryingUpdate, timeEval.TimeCode, OmniConnectTokens->widths, &UsdGeomType::CreateWidthsAttr);

    if (performsUpdate)
    {
      UsdGeomType& outGeom = timeVaryingUpdate ? clipGeom : actGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::SCALES);

      UsdAttribute widthsAttribute = outGeom.GetWidthsAttr();
      assert(widthsAttribute);
      if (omniGeomData.Scales)
      {
        VtFloatArray usdWidths;
        void* arrayData = omniGeomData.Scales;
        size_t arrayNumElements = omniGeomData.NumPoints;
        UsdAttribute arrayPrimvar = widthsAttribute;
        bool setPrimvar = false;
        switch (omniGeomData.ScalesType)
        {
        case OmniConnectType::FLOAT: {ASSIGN_CUSTOM_ARRAY_TO_PRIMVAR_MACRO(VtFloatArray, usdWidths); break; }
        case OmniConnectType::DOUBLE: {ASSIGN_CUSTOM_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtFloatArray, double, usdWidths); break; }
        case OmniConnectType::FLOAT3: {ASSIGN_CUSTOM_ARRAY_TO_PRIMVAR_FIRST_COMPONENT_MACRO(VtFloatArray, GfVec3f, usdWidths); break; }
        case OmniConnectType::DOUBLE3: {ASSIGN_CUSTOM_ARRAY_TO_PRIMVAR_FIRST_COMPONENT_MACRO(VtFloatArray, GfVec3d, usdWidths); break; }
        default: { widthsAttribute.ClearAtTime(timeCode); OmniConnectErrorMacro("UsdGeom WidthsAttr does not support type: " << omniGeomData.ScalesType) break; }
        }

        for(size_t i = 0; i < usdWidths.size(); ++i)
          usdWidths[i] *= GetShapeWidth(omniGeomData); // omniGeomData typically contains the radius, whereas 'widths' a diameter

        arrayPrimvar.Set(usdWidths, timeCode);
      }
      else
      {
        float pointScale = static_cast<float>(omniGeomData.UniformScale);
        VtFloatArray usdWidths(omniGeomData.NumPoints, pointScale);
        widthsAttribute.Set(usdWidths, timeCode);
      }
    }
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomScales(UsdGeomType& actGeom, UsdGeomType& clipGeom, UsdGeomType& topGeom, const GeomDataType& omniGeomData, uint64_t numPrims,
    OmniConnectUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
  {
    using DMI = typename GeomDataType::DataMemberId;
    bool performsUpdate = updateEval.PerformsUpdate(DMI::SCALES);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::SCALES);

    SyncTimeVaryingAttribute(actGeom.GetScalesAttr(), clipGeom.GetScalesAttr(), topGeom,
      timeVaryingUpdate, timeEval.TimeCode, OmniConnectTokens->scales, &UsdGeomType::CreateScalesAttr);

    if (performsUpdate)
    {
      UsdGeomType& outGeom = timeVaryingUpdate ? clipGeom : actGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::SCALES);

      UsdAttribute scalesAttribute = outGeom.GetScalesAttr();
      assert(scalesAttribute);
      if (omniGeomData.Scales)
      {
        void* arrayData = omniGeomData.Scales;
        size_t arrayNumElements = omniGeomData.NumPoints;
        UsdAttribute arrayPrimvar = scalesAttribute;
        bool setPrimvar = true;
        switch (omniGeomData.ScalesType)
        {
        case OmniConnectType::FLOAT3: {ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec3fArray); break; }
        case OmniConnectType::DOUBLE3: {ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtVec3fArray, GfVec3d); break; }
        default: { scalesAttribute.ClearAtTime(timeCode); OmniConnectErrorMacro("UsdGeom ScalesAttr does not support type: " << omniGeomData.ScalesType) break; }
        }
      }
      else
      {
        double pointScale = omniGeomData.UniformScale;
        GfVec3f defaultScale((float)pointScale, (float)pointScale, (float)pointScale);
        VtVec3fArray usdScales(omniGeomData.NumPoints, defaultScale);
        scalesAttribute.Set(usdScales, timeCode);
      }
    }
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomOrientNormals(UsdGeomType& actGeom, UsdGeomType& clipGeom, UsdGeomType& topGeom, const GeomDataType& omniGeomData, uint64_t numPrims,
    OmniConnectUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
  {
    using DMI = typename GeomDataType::DataMemberId;
    bool performsUpdate = updateEval.PerformsUpdate(DMI::ORIENTATIONS);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::ORIENTATIONS);

    SyncTimeVaryingAttribute(actGeom.GetNormalsAttr(), clipGeom.GetNormalsAttr(), topGeom,
      timeVaryingUpdate, timeEval.TimeCode, OmniConnectTokens->normals, &UsdGeomType::CreateNormalsAttr);

    if (performsUpdate)
    {
      UsdGeomType& outGeom = timeVaryingUpdate ? clipGeom : actGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::ORIENTATIONS);

      UsdAttribute normalsAttribute = outGeom.GetNormalsAttr();
      assert(normalsAttribute);
      if (omniGeomData.Orientations)
      {
        void* arrayData = omniGeomData.Orientations;
        size_t arrayNumElements = omniGeomData.NumPoints;
        UsdAttribute arrayPrimvar = normalsAttribute;
        bool setPrimvar = true;
        switch (omniGeomData.OrientationsType)
        {
        case OmniConnectType::FLOAT3: {ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtVec3fArray); break; }
        case OmniConnectType::DOUBLE3: {ASSIGN_ARRAY_TO_PRIMVAR_CONVERT_MACRO(VtVec3fArray, GfVec3d); break; }
        default: { normalsAttribute.ClearAtTime(timeCode); OmniConnectErrorMacro("UsdGeom NormalsAttr (for points) does not support type: " << omniGeomData.OrientationsType) break; }
        }
      }
      else
      {
        //Always provide a default orientation
        GfVec3f defaultNormal(1, 0, 0);
        VtVec3fArray usdNormals(omniGeomData.NumPoints, defaultNormal);
        normalsAttribute.Set(usdNormals, timeCode);
      } 
    }
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomOrientations(UsdGeomType& actGeom, UsdGeomType& clipGeom, UsdGeomType& topGeom, const GeomDataType& omniGeomData, uint64_t numPrims,
    OmniConnectUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
  {
    using DMI = typename GeomDataType::DataMemberId;
    bool performsUpdate = updateEval.PerformsUpdate(DMI::ORIENTATIONS);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::ORIENTATIONS);

    SyncTimeVaryingAttribute(actGeom.GetOrientationsAttr(), clipGeom.GetOrientationsAttr(), topGeom,
      timeVaryingUpdate, timeEval.TimeCode, OmniConnectTokens->orientations, &UsdGeomType::CreateOrientationsAttr);

    if (performsUpdate)
    {
      UsdGeomType& outGeom = timeVaryingUpdate ? clipGeom : actGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::ORIENTATIONS);

      // Orientations
      UsdAttribute orientationsAttribute = outGeom.GetOrientationsAttr();
      assert(orientationsAttribute);
      if (omniGeomData.Orientations)
      {
        //VtQuathArray usdOrients(omniGeomData.NumPoints);
        //switch (omniGeomData.OrientationsType)
        //{
        //case OmniConnectType::FLOAT3: { ConvertNormalsToQuaternions<float>(usdOrients, omniGeomData.Orientations, omniGeomData.NumPoints); break; }
        //case OmniConnectType::DOUBLE3: { ConvertNormalsToQuaternions<double>(usdOrients, omniGeomData.Orientations, omniGeomData.NumPoints); break; }
        //default: { orientationsAttribute.ClearAtTime(timeCode); OmniConnectErrorMacro("UsdGeom OrientationsAttr does not support type: " << omniGeomData.OrientationsType) break; }
        //}
        //orientationsAttribute.Set(usdOrients, timeCode);

        assert(omniGeomData.OrientationsType == OmniConnectType::FLOAT4);

        VtQuathArray usdOrients(omniGeomData.NumPoints);
        for (uint64_t i = 0; i < omniGeomData.NumPoints; ++i)
        {
          float* orients = reinterpret_cast<float*>(omniGeomData.Orientations);
          usdOrients[i] = GfQuath(orients[i * 4], orients[i * 4 + 1], orients[i * 4 + 2], orients[i * 4 + 3]);
        }
        orientationsAttribute.Set(usdOrients, timeCode);
      }
      else
      {
        //Always provide a default orientation
        GfQuath defaultOrient(1, 0, 0, 0);
        VtQuathArray usdOrients(omniGeomData.NumPoints, defaultOrient);
        orientationsAttribute.Set(usdOrients, timeCode);
      }
    }
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomShapeIndices(UsdGeomType& actGeom, UsdGeomType& clipGeom, UsdGeomType& topGeom, const GeomDataType& omniGeomData, uint64_t numPrims,
    OmniConnectUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
  {
    using DMI = typename GeomDataType::DataMemberId;
    bool performsUpdate = updateEval.PerformsUpdate(DMI::SHAPEINDICES);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::SHAPEINDICES);

    SyncTimeVaryingAttribute(actGeom.GetProtoIndicesAttr(), clipGeom.GetProtoIndicesAttr(), topGeom,
      timeVaryingUpdate, timeEval.TimeCode, OmniConnectTokens->protoIndices, &UsdGeomType::CreateProtoIndicesAttr);

    if (performsUpdate)
    {
      UsdGeomType& outGeom = timeVaryingUpdate ? clipGeom : actGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::SHAPEINDICES);

      //Shape indices
      UsdAttribute protoIndexAttr = outGeom.GetProtoIndicesAttr();
      assert(protoIndexAttr);
      VtIntArray protoIndices;
      if (omniGeomData.ShapeIndices)
      {
        protoIndices.assign(omniGeomData.ShapeIndices, omniGeomData.ShapeIndices + omniGeomData.NumPoints);
      }
      else
      {
        assert(omniGeomData.NumShapes == 1);
        protoIndices.assign(omniGeomData.NumPoints, 0);
      }
      protoIndexAttr.Set(protoIndices, timeCode);
    }
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomLinearVelocities(UsdGeomType& actGeom, UsdGeomType& clipGeom, UsdGeomType& topGeom, const GeomDataType& omniGeomData, uint64_t numPrims,
    OmniConnectUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
  {
    using DMI = typename GeomDataType::DataMemberId;
    bool performsUpdate = updateEval.PerformsUpdate(DMI::LINEARVELOCITIES);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::LINEARVELOCITIES);

    SyncTimeVaryingAttribute(actGeom.GetVelocitiesAttr(), clipGeom.GetVelocitiesAttr(), topGeom,
      timeVaryingUpdate, timeEval.TimeCode, OmniConnectTokens->velocities, &UsdGeomType::CreateVelocitiesAttr);

    if (performsUpdate)
    {
      UsdGeomType& outGeom = timeVaryingUpdate ? clipGeom : actGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::LINEARVELOCITIES);

      // Linear velocities
      UsdAttribute linearVelocitiesAttribute = outGeom.GetVelocitiesAttr();
      assert(linearVelocitiesAttribute);
      if (omniGeomData.LinearVelocities)
      {
        GfVec3f* linVels = (GfVec3f*)omniGeomData.LinearVelocities;

        VtVec3fArray usdVelocities;
        usdVelocities.assign(linVels, linVels + omniGeomData.NumPoints);
        linearVelocitiesAttribute.Set(usdVelocities, timeCode);
      }
      else
      {
        linearVelocitiesAttribute.Set(SdfValueBlock(), timeCode);
      }
    }
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomAngularVelocities(UsdGeomType& actGeom, UsdGeomType& clipGeom, UsdGeomType& topGeom, const GeomDataType& omniGeomData, uint64_t numPrims,
    OmniConnectUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
  {
    using DMI = typename GeomDataType::DataMemberId;
    bool performsUpdate = updateEval.PerformsUpdate(DMI::ANGULARVELOCITIES);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::ANGULARVELOCITIES);

    SyncTimeVaryingAttribute(actGeom.GetAngularVelocitiesAttr(), clipGeom.GetAngularVelocitiesAttr(), topGeom,
      timeVaryingUpdate, timeEval.TimeCode, OmniConnectTokens->angularVelocities, &UsdGeomType::CreateAngularVelocitiesAttr);

    if (performsUpdate)
    {
      UsdGeomType& outGeom = timeVaryingUpdate ? clipGeom : actGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::ANGULARVELOCITIES);

      // Angular velocities
      UsdAttribute angularVelocitiesAttribute = outGeom.GetAngularVelocitiesAttr();
      assert(angularVelocitiesAttribute);
      if (omniGeomData.AngularVelocities)
      {
        GfVec3f* angVels = (GfVec3f*)omniGeomData.AngularVelocities;

        VtVec3fArray usdAngularVelocities;
        usdAngularVelocities.assign(angVels, angVels + omniGeomData.NumPoints);
        angularVelocitiesAttribute.Set(usdAngularVelocities, timeCode);
      }
      else
      {
        angularVelocitiesAttribute.Set(SdfValueBlock(), timeCode);
      }
    }
  }

  template<typename UsdGeomType, typename GeomDataType>
  void UpdateUsdGeomInvisibleIndices(UsdGeomType& actGeom, UsdGeomType& clipGeom, UsdGeomType& topGeom, const GeomDataType& omniGeomData, uint64_t numPrims,
    OmniConnectUpdateEvaluator<const GeomDataType>& updateEval, TimeEvaluator<GeomDataType>& timeEval)
  {
    using DMI = typename GeomDataType::DataMemberId;
    bool performsUpdate = updateEval.PerformsUpdate(DMI::INVISIBLEINDICES);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::INVISIBLEINDICES);

    SyncTimeVaryingAttribute(actGeom.GetInvisibleIdsAttr(), clipGeom.GetInvisibleIdsAttr(), topGeom,
      timeVaryingUpdate, timeEval.TimeCode, OmniConnectTokens->invisibleIds, &UsdGeomType::CreateInvisibleIdsAttr);

    if (performsUpdate)
    {
      UsdGeomType& outGeom = timeVaryingUpdate ? clipGeom : actGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::INVISIBLEINDICES);

      // Invisible indices
      UsdAttribute invisIndexAttr = outGeom.GetInvisibleIdsAttr();
      assert(invisIndexAttr);
      uint64_t numInvisibleIndices = omniGeomData.NumInvisibleIndices;
      if (numInvisibleIndices)
      {
        VtArray<int64_t> usdInvisibleIndices(numInvisibleIndices);
        for (uint64_t i = 0; i < numInvisibleIndices; ++i)
          usdInvisibleIndices[i] = omniGeomData.InvisibleIndices[i];

        invisIndexAttr.Set(usdInvisibleIndices, timeCode);
      }
      else
      {
        invisIndexAttr.Set(SdfValueBlock(), timeCode);
      }
    }
  }

  static void UpdateUsdGeomShapes(OmniConnectActorCache& actorCache, OmniConnectInstancerCache& instancerCache, OmniConnectInstancerData& omniInstancerData,
    UsdGeomPointInstancer& instancer, OmniConnectUpdateEvaluator<const OmniConnectInstancerData>& updateEval, bool newInstancer)
  {
    using DMI = typename OmniConnectInstancerData::DataMemberId;

    bool performsUpdate = updateEval.PerformsUpdate(DMI::SHAPES);
    if (newInstancer || performsUpdate)
    {
      //Shapes
      UsdRelationship protoRel = instancer.GetPrototypesRel();
      protoRel.ClearTargets(false);
      UsdPrimSiblingRange children = actorCache.Stage->GetPrimAtPath(instancerCache.SdfProtoPath).GetAllChildren();
      for (UsdPrim child : children)
      {
        actorCache.Stage->RemovePrim(child.GetPath()); // Remove all prototype shapes
      }
      for (int i = 0; i < omniInstancerData.NumShapes; ++i)
      {
        SdfPath protoShapePath;
        switch (omniInstancerData.Shapes[i])
        {
          case OmniConnectInstancerData::SHAPE_SPHERE:
          {
            protoShapePath = instancerCache.SdfProtoPath.AppendPath(instancerCache.RelSpherePath);
            UsdGeomSphere geomSphere = UsdGeomSphere::Define(actorCache.Stage, protoShapePath);

            UsdGeomXformOp scaleOp = geomSphere.AddScaleOp();
            scaleOp.Set(GfVec3f(omniInstancerData.ShapeDims[0]));
            break;
          }
          case OmniConnectInstancerData::SHAPE_CYLINDER:
          {
            protoShapePath = instancerCache.SdfProtoPath.AppendPath(instancerCache.RelCylinderPath);
            UsdGeomCylinder geomCylinder = UsdGeomCylinder::Define(actorCache.Stage, protoShapePath);   
            UsdGeomXformOp rotateOp = geomCylinder.AddRotateXOp();
            rotateOp.Set(90.0f);
            UsdGeomXformOp scaleOp = geomCylinder.AddScaleOp();
            scaleOp.Set(GfVec3f(omniInstancerData.ShapeDims[1], omniInstancerData.ShapeDims[1], omniInstancerData.ShapeDims[0]));
            break;
          }
          case OmniConnectInstancerData::SHAPE_CONE:
          {
            protoShapePath = instancerCache.SdfProtoPath.AppendPath(instancerCache.RelConePath);
            UsdGeomCone geomCone = UsdGeomCone::Define(actorCache.Stage, protoShapePath);
            UsdGeomXformOp rotateOp = geomCone.AddRotateYOp();
            rotateOp.Set(90.0f);
            UsdGeomXformOp scaleOp = geomCone.AddScaleOp();
            scaleOp.Set(GfVec3f(omniInstancerData.ShapeDims[1], omniInstancerData.ShapeDims[1], omniInstancerData.ShapeDims[0]));
            break;
          }
          case OmniConnectInstancerData::SHAPE_CUBE:
          {
            protoShapePath = instancerCache.SdfProtoPath.AppendPath(instancerCache.RelCubePath);
            UsdGeomCube geomCube = UsdGeomCube::Define(actorCache.Stage, protoShapePath);
            UsdGeomXformOp scaleOp = geomCube.AddScaleOp();
            scaleOp.Set(GfVec3f(omniInstancerData.ShapeDims));
            break;
          }
          case OmniConnectInstancerData::SHAPE_ARROW:
          {
            protoShapePath = instancerCache.SdfProtoPath.AppendPath(instancerCache.RelArrowPath);
            UsdGeomXform arrowXform = UsdGeomXform::Define(actorCache.Stage, protoShapePath);

            // Length of this shape is always 1, with a ratio between cylinder and cone defining the shape
            float conePercentage = omniInstancerData.ShapeDims[0];
            float cylRadius = omniInstancerData.ShapeDims[1];
            float coneRadius = omniInstancerData.ShapeDims[2];
            {
              SdfPath cylinderPath = protoShapePath.AppendPath(instancerCache.RelCylinderPath);
              UsdGeomCylinder geomCylinder = UsdGeomCylinder::Define(actorCache.Stage, cylinderPath);  

              UsdGeomXformOp rotateOp = geomCylinder.AddRotateYOp();
              rotateOp.Set(90.0f);
              UsdGeomXformOp scaleOp = geomCylinder.AddScaleOp();
              scaleOp.Set(GfVec3f(cylRadius, cylRadius, 0.5f*(1.0f-conePercentage))); // from length=2 to final length of cylinder (1.0f-conePercentage)
              UsdGeomXformOp transOp = geomCylinder.AddTranslateOp();
              transOp.Set(GfVec3d(0, 0, 1)); // start at origin
            }

            {
              SdfPath conePath = protoShapePath.AppendPath(instancerCache.RelConePath);
              UsdGeomCone geomCone = UsdGeomCone::Define(actorCache.Stage, conePath);

              float halfConeLength = 0.5f*conePercentage;
              UsdGeomXformOp rotateOp = geomCone.AddRotateYOp();
              rotateOp.Set(90.0f);
              UsdGeomXformOp transOp = geomCone.AddTranslateOp();
              transOp.Set(GfVec3d(0.0, 0.0, 1.0-halfConeLength)); // offset to origin (halfConeLength), then offset across length of cylinder (1.0f-conePercentage)
              UsdGeomXformOp scaleOp = geomCone.AddScaleOp();
              scaleOp.Set(GfVec3f(coneRadius, coneRadius, halfConeLength)); // scale from length=2 to the cone's length (conePercentage)
            }
            break;
          }
          default:
          {
            std::string meshIdstr = std::to_string(size_t(omniInstancerData.Shapes[i]));
            SdfPath meshPath(actorCache.MeshBaseName + meshIdstr);
            SdfPath overMeshPath(instancerCache.ProtoPath + "/Mesh_" + meshIdstr);

            UsdPrim overPrim = actorCache.Stage->DefinePrim(overMeshPath);
            overPrim.GetReferences().AddInternalReference(meshPath);
            UsdGeomMesh overMesh(overPrim);
            overMesh.CreateVisibilityAttr(VtValue("inherited"));
            UsdGeomXformOp scaleOp = overMesh.AddScaleOp();
            scaleOp.Set(GfVec3f(omniInstancerData.ShapeDims));
            UsdGeomMesh refMesh = UsdGeomMesh::Get(actorCache.Stage, meshPath);
            refMesh.MakeInvisible();
            //(in)active controls visibility for traversal, in essence an alternative "delete"
            //overMesh.SetActive(true);
            //actorCache.Stage->GetPrimAtPath(meshPath).SetActive(false); 

            protoShapePath = overMeshPath;
            break;
          }
        }
        protoRel.AddTarget(protoShapePath);
      }
    }
  }

  void UpdateUsdGeomCurveLengths(UsdGeomBasisCurves& actGeom, UsdGeomBasisCurves& clipGeom, UsdGeomBasisCurves& topGeom, const OmniConnectCurveData& omniGeomData, uint64_t numPrims,
    OmniConnectUpdateEvaluator<const OmniConnectCurveData>& updateEval, TimeEvaluator<OmniConnectCurveData>& timeEval)
  {
    using DMI = typename OmniConnectCurveData::DataMemberId;
    // Fill geom prim and geometry layer with data.
    bool performsUpdate = updateEval.PerformsUpdate(DMI::CURVELENGTHS);
    bool timeVaryingUpdate = timeEval.IsTimeVarying(DMI::CURVELENGTHS);

    SyncTimeVaryingAttribute(actGeom.GetCurveVertexCountsAttr(), clipGeom.GetCurveVertexCountsAttr(), topGeom,
      timeVaryingUpdate, timeEval.TimeCode, OmniConnectTokens->curveVertexCounts, &UsdGeomBasisCurves::CreateCurveVertexCountsAttr);

    if (performsUpdate)
    {
      UsdGeomBasisCurves& outGeom = timeVaryingUpdate ? clipGeom : actGeom;
      UsdTimeCode timeCode = timeEval.Eval(DMI::POINTS);

      UsdAttribute vertCountAttr = outGeom.GetCurveVertexCountsAttr();
      assert(vertCountAttr);

      void* arrayData = omniGeomData.CurveLengths;
      size_t arrayNumElements = omniGeomData.NumCurveLengths;
      UsdAttribute arrayPrimvar = vertCountAttr;
      VtVec3fArray usdVerts;
      bool setPrimvar = true;
      { ASSIGN_ARRAY_TO_PRIMVAR_MACRO(VtIntArray); }
    }
  }
}

#define UPDATE_USDGEOM_MESH(FuncDef) \
  FuncDef(meshActorGeom, meshClipGeom, meshTopGeom, omniMeshData, numPrims, updateEval, timeEval)
#define UPDATE_USDGEOM_MESH_PRIMVARS(FuncDef) \
  FuncDef(actPrimvarsApi, clipPrimvarsApi, topPrimvarsApi, omniMeshData, numPrims, updateEval, timeEval)

bool OmniConnectInternals::UpdateActorGeom(OmniConnectActorCache& actorCache, OmniConnectMeshCache& meshCache, const OmniConnectMeshData& omniMeshData, double animTimeStep,
  OmniConnectGenericArray* updatedGenericArrays, size_t numUga, OmniConnectGenericArray* deletedGenericArrays, size_t numDga)
{
  OmniConnectUpdateEvaluator<const OmniConnectMeshData> updateEval(omniMeshData);
  TimeEvaluator<OmniConnectMeshData> timeEval(omniMeshData, animTimeStep);

  // Initialize the mesh prims at the various stages
  UsdGeomMesh meshActorGeom;
  UsdGeomMesh meshClipGeom;
  UsdGeomMesh meshTopGeom;
  UsdStageRefPtr geomClipStage, geomTopologyStage;

  bool newMesh;
  bool newTimeStep;

  GetActorGeomPrimsAndStages(actorCache, meshCache, animTimeStep,
    meshActorGeom, meshClipGeom, meshTopGeom, geomClipStage, geomTopologyStage, newMesh, newTimeStep);

  UsdGeomPrimvarsAPI actPrimvarsApi(meshActorGeom), clipPrimvarsApi(meshClipGeom), topPrimvarsApi(meshTopGeom);

  assert((omniMeshData.NumIndices % 3) == 0);
  uint64_t numPrims = int(omniMeshData.NumIndices) / 3;

  UPDATE_USDGEOM_MESH(UpdateUsdGeomPoints);
  UPDATE_USDGEOM_MESH(UpdateUsdGeomNormals);
  UPDATE_USDGEOM_MESH_PRIMVARS(UpdateUsdGeomTexCoords);
  UPDATE_USDGEOM_MESH_PRIMVARS(UpdateUsdGeomColors);
  UPDATE_USDGEOM_MESH(UpdateUsdGeomIndices);

  UpdateGenericArrays(actPrimvarsApi, clipPrimvarsApi, topPrimvarsApi, updatedGenericArrays, numUga, animTimeStep);
  DeleteGenericArrays(actPrimvarsApi, clipPrimvarsApi, topPrimvarsApi, deletedGenericArrays, numDga, animTimeStep);

  if (UsdSaveEnabled)
  {
    geomTopologyStage->Save();
    geomClipStage->Save();
  }

  return newMesh;
}

#define UPDATE_USDGEOM_POINTS(FuncDef) \
  FuncDef(geomPointsActor, geomPointsClip, geomPointsTop, omniInstancerData, numPrims, updateEval, timeEval)
#define UPDATE_USDGEOM_POINTS_PRIMVARS(FuncDef) \
  FuncDef(actPrimvarsApi, clipPrimvarsApi, topPrimvarsApi, omniInstancerData, numPrims, updateEval, timeEval)

bool OmniConnectInternals::UpdateActorGeom(OmniConnectActorCache& actorCache, OmniConnectInstancerCache& instancerCache, OmniConnectInstancerData& omniInstancerData, double animTimeStep,
  OmniConnectGenericArray* updatedGenericArrays, size_t numUga, OmniConnectGenericArray* deletedGenericArrays, size_t numDga)
{
  OmniConnectUpdateEvaluator<const OmniConnectInstancerData> updateEval(omniInstancerData);
  TimeEvaluator<OmniConnectInstancerData> timeEval(omniInstancerData, animTimeStep);

  bool simpleSphereInstancer = omniInstancerData.NumShapes == 1 && omniInstancerData.Shapes[0] == OmniConnectInstancerData::SHAPE_SPHERE;
  bool useGeomPoints = instancerCache.UsesAltUsdPrimType = !Settings.UsePointInstancer && simpleSphereInstancer;

  UsdStageRefPtr geomClipStage, geomTopologyStage;

  bool newInstancer;
  bool newTimeStep;

  uint64_t numPrims = omniInstancerData.NumPoints;

  if (useGeomPoints)
  {
    UsdGeomPoints geomPointsActor;
    UsdGeomPoints geomPointsClip;
    UsdGeomPoints geomPointsTop;

    GetActorGeomPrimsAndStages(actorCache, instancerCache, animTimeStep,
      geomPointsActor, geomPointsClip, geomPointsTop, geomClipStage, geomTopologyStage, newInstancer, newTimeStep);

    UsdGeomPrimvarsAPI actPrimvarsApi(geomPointsActor), clipPrimvarsApi(geomPointsClip), topPrimvarsApi(geomPointsTop);

    UPDATE_USDGEOM_POINTS(UpdateUsdGeomPoints);
    UPDATE_USDGEOM_POINTS(UpdateUsdGeomInstanceIds);
    UPDATE_USDGEOM_POINTS(UpdateUsdGeomWidths);
    UPDATE_USDGEOM_POINTS(UpdateUsdGeomOrientNormals);
    UPDATE_USDGEOM_POINTS_PRIMVARS(UpdateUsdGeomTexCoords);

    UpdateUsdGeomColors(actPrimvarsApi, clipPrimvarsApi, topPrimvarsApi, omniInstancerData, numPrims, updateEval, timeEval
#if defined(USE_MDL_MATERIALS) && USE_CUSTOM_POINT_SHADER
      , true
#endif
    );

    UpdateGenericArrays(actPrimvarsApi, clipPrimvarsApi, topPrimvarsApi, updatedGenericArrays, numUga, animTimeStep);
    DeleteGenericArrays(actPrimvarsApi, clipPrimvarsApi, topPrimvarsApi, deletedGenericArrays, numDga, animTimeStep);
  }
  else
  {
    UsdGeomPointInstancer geomPointsActor;
    UsdGeomPointInstancer geomPointsClip;
    UsdGeomPointInstancer geomPointsTop;

    GetActorGeomPrimsAndStages(actorCache, instancerCache, animTimeStep,
      geomPointsActor, geomPointsClip, geomPointsTop, geomClipStage, geomTopologyStage, newInstancer, newTimeStep);

    UsdGeomPrimvarsAPI actPrimvarsApi(geomPointsActor), clipPrimvarsApi(geomPointsClip), topPrimvarsApi(geomPointsTop);

    if (newInstancer)
    {
      actorCache.Stage->OverridePrim(instancerCache.SdfProtoPath);
    }

    UpdateUsdGeomShapes(actorCache, instancerCache, omniInstancerData, geomPointsActor, updateEval, newInstancer);

    UPDATE_USDGEOM_POINTS(UpdateUsdGeomPoints);
    UPDATE_USDGEOM_POINTS(UpdateUsdGeomInstanceIds);
    UPDATE_USDGEOM_POINTS(UpdateUsdGeomScales);
    UPDATE_USDGEOM_POINTS(UpdateUsdGeomOrientations);
    UPDATE_USDGEOM_POINTS_PRIMVARS(UpdateUsdGeomTexCoords);
    UPDATE_USDGEOM_POINTS_PRIMVARS(UpdateUsdGeomColors);
    UPDATE_USDGEOM_POINTS(UpdateUsdGeomShapeIndices);
    UPDATE_USDGEOM_POINTS(UpdateUsdGeomLinearVelocities);
    UPDATE_USDGEOM_POINTS(UpdateUsdGeomAngularVelocities);
    UPDATE_USDGEOM_POINTS(UpdateUsdGeomInvisibleIndices);

    UpdateGenericArrays(actPrimvarsApi, clipPrimvarsApi, topPrimvarsApi, updatedGenericArrays, numUga, animTimeStep);
    DeleteGenericArrays(actPrimvarsApi, clipPrimvarsApi, topPrimvarsApi, deletedGenericArrays, numDga, animTimeStep);
  }

  if (UsdSaveEnabled)
  {
    geomTopologyStage->Save();
    geomClipStage->Save();
  }

  return newInstancer;
}

#define UPDATE_USDGEOM_CURVE(FuncDef) \
  FuncDef(curveActorGeom, curveClipGeom, curveTopGeom, omniCurveData, numPrims, updateEval, timeEval)
#define UPDATE_USDGEOM_CURVE_PRIMVARS(FuncDef) \
  FuncDef(actPrimvarsApi, clipPrimvarsApi, topPrimvarsApi, omniCurveData, numPrims, updateEval, timeEval)

bool OmniConnectInternals::UpdateActorGeom(OmniConnectActorCache & actorCache, OmniConnectCurveCache & curveCache, const OmniConnectCurveData & omniCurveData, double animTimeStep, OmniConnectGenericArray * updatedGenericArrays, size_t numUga, OmniConnectGenericArray * deletedGenericArrays, size_t numDga)
{
  OmniConnectUpdateEvaluator<const OmniConnectCurveData> updateEval(omniCurveData);
  TimeEvaluator<OmniConnectCurveData> timeEval(omniCurveData, animTimeStep);

  // Initialize the curve prims at the various stages
  UsdGeomBasisCurves curveActorGeom;
  UsdGeomBasisCurves curveClipGeom;
  UsdGeomBasisCurves curveTopGeom;
  UsdStageRefPtr geomClipStage, geomTopologyStage;

  bool newCurve;
  bool newTimeStep;

  GetActorGeomPrimsAndStages(actorCache, curveCache, animTimeStep,
    curveActorGeom, curveClipGeom, curveTopGeom, geomClipStage, geomTopologyStage, newCurve, newTimeStep);

  UsdGeomPrimvarsAPI actPrimvarsApi(curveActorGeom), clipPrimvarsApi(curveClipGeom), topPrimvarsApi(curveTopGeom);

  uint64_t numPrims = omniCurveData.NumCurveLengths;

  UPDATE_USDGEOM_CURVE(UpdateUsdGeomPoints);
  UPDATE_USDGEOM_CURVE(UpdateUsdGeomNormals);
  UPDATE_USDGEOM_CURVE_PRIMVARS(UpdateUsdGeomTexCoords);
  UPDATE_USDGEOM_CURVE_PRIMVARS(UpdateUsdGeomColors);
  UPDATE_USDGEOM_CURVE(UpdateUsdGeomWidths);
  UPDATE_USDGEOM_CURVE(UpdateUsdGeomCurveLengths);

  UpdateGenericArrays(actPrimvarsApi, clipPrimvarsApi, topPrimvarsApi, updatedGenericArrays, numUga, animTimeStep);
  DeleteGenericArrays(actPrimvarsApi, clipPrimvarsApi, topPrimvarsApi, deletedGenericArrays, numDga, animTimeStep);

  if (UsdSaveEnabled)
  {
    geomTopologyStage->Save();
    geomClipStage->Save();
  }

  return newCurve;
}

#define MANAGE_SET_AND_LINK_VOLUME_FIELD(Path, RelToken, GridToken, Create) \
  ManageSetAndLinkOpenVDBAsset(actorCache.Stage, geomClipStage, geomTopologyStage, volActGeom, \
      Path, RelToken, GridToken, Create, volAsset, \
      performsUpdate, timeVaryingUpdate, timeCode)

bool OmniConnectInternals::UpdateActorGeom(OmniConnectActorCache& actorCache, OmniConnectVolumeCache& volumeCache, OmniConnectVolumeData& omniVolumeData, double animTimeStep,
  OmniConnectGenericArray* updatedGenericArrays, size_t numUga, OmniConnectGenericArray* deletedGenericArrays, size_t numDga)
{
  using DMI = typename OmniConnectVolumeData::DataMemberId;

  OmniConnectUpdateEvaluator<const OmniConnectVolumeData> updateEval(omniVolumeData);
  TimeEvaluator<OmniConnectVolumeData> timeEval(omniVolumeData, animTimeStep);

  bool useMeshVolume = volumeCache.HasPrivateMaterial = volumeCache.UsesAltUsdPrimType = Settings.UseMeshVolume;

  UsdStageRefPtr geomClipStage, geomTopologyStage;

  bool newVolume;
  bool newTimeStep;

  bool performsUpdate;
  bool timeVaryingUpdate;

  UsdGeomGprim geomActPrim;
  UsdGeomGprim geomClipPrim;
  UsdGeomGprim geomTopPrim;
  UsdGeomGprim geomOutPrim;

  performsUpdate = updateEval.PerformsUpdate(DMI::DATA);
  timeVaryingUpdate = timeEval.IsTimeVarying(DMI::DATA);
  
  UsdTimeCode timeCode = timeEval.Eval(DMI::DATA);

  // Geometry-space coordinates (as if the volume were a pointset with the following min and max values)
  float minX = omniVolumeData.Origin[0];
  float minY = omniVolumeData.Origin[1];
  float minZ = omniVolumeData.Origin[2];
  float maxX = ((float)omniVolumeData.NumElements[0] * omniVolumeData.CellDimensions[0]) + minX;
  float maxY = ((float)omniVolumeData.NumElements[1] * omniVolumeData.CellDimensions[1]) + minY;
  float maxZ = ((float)omniVolumeData.NumElements[2] * omniVolumeData.CellDimensions[2]) + minZ;

  if (useMeshVolume)
  {
    UsdGeomMesh volActGeom;
    UsdGeomMesh volClipGeom;
    UsdGeomMesh volTopGeom;

    GetActorGeomPrimsAndStages(actorCache, volumeCache, animTimeStep,
      volActGeom, volClipGeom, volTopGeom, geomClipStage, geomTopologyStage, newVolume, newTimeStep);

    UsdGeomPrimvarsAPI actPrimvarsApi(volActGeom), clipPrimvarsApi(volClipGeom), topPrimvarsApi(volTopGeom);

    UsdGeomMesh& volOutGeom = timeVaryingUpdate ? volClipGeom : volTopGeom;
    geomActPrim = volActGeom;
    geomClipPrim = volClipGeom;
    geomTopPrim = volTopGeom;
    geomOutPrim = volOutGeom;

    SyncTimeVaryingAttribute(volActGeom.GetPointsAttr(), volClipGeom.GetPointsAttr(), volTopGeom,
      timeVaryingUpdate, timeCode, OmniConnectTokens->points, &UsdGeomMesh::CreatePointsAttr);

#ifdef USE_MDL_MATERIALS
    UsdShadeShader volActShader = UsdShadeShader::Get(actorCache.Stage, volumeCache.MeshVolShadPath);
    UsdShadeShader volClipShader = UsdShadeShader::Get(geomClipStage, volumeCache.MeshVolShadPath);
    UsdShadeShader volTopShader = UsdShadeShader::Get(geomTopologyStage, volumeCache.MeshVolShadPath);

    SyncTimeVaryingInput(volActShader, volClipShader, volTopShader, timeVaryingUpdate, timeCode, 
      OmniConnectTokens->volume_density_texture, "inputs:volume_density_texture", SdfValueTypeNames->Asset);
#endif

    if (performsUpdate)
    {
      VtVec3fArray points({
        GfVec3f(minX, minY, minZ), GfVec3f(maxX, minY, minZ), 
        GfVec3f(minX, minY, maxZ), GfVec3f(maxX, minY, maxZ), 
        GfVec3f(minX, maxY, minZ), GfVec3f(maxX, maxY, minZ), 
        GfVec3f(maxX, maxY, maxZ), GfVec3f(minX, maxY, maxZ)
      });
      volOutGeom.GetPointsAttr().Set(points, timeCode);

#ifdef USE_MDL_MATERIALS
      UsdShadeShader volOutShader = timeVaryingUpdate ? volClipShader : volActShader;
      volumeCache.ResetTimedOvdbFile(animTimeStep);

      SdfAssetPath volAsset(volumeCache.TimedRelOvdbFile);
      volOutShader.GetInput(OmniConnectTokens->volume_density_texture).Set(volAsset, timeCode);
#endif
    }
  }
  else
  {
    UsdVolVolume volActGeom;
    UsdVolVolume volClipGeom;
    UsdVolVolume volTopGeom;

    GetActorGeomPrimsAndStages(actorCache, volumeCache, animTimeStep,
      volActGeom, volClipGeom, volTopGeom, geomClipStage, geomTopologyStage, newVolume, newTimeStep);

    UsdGeomPrimvarsAPI actPrimvarsApi(volActGeom), clipPrimvarsApi(volClipGeom), topPrimvarsApi(volTopGeom);

    UsdVolVolume& volOutGeom = timeVaryingUpdate ? volClipGeom : volActGeom;
    geomActPrim = volActGeom;
    geomClipPrim = volClipGeom;
    geomTopPrim = volTopGeom;
    geomOutPrim = volOutGeom;

    volumeCache.ResetTimedOvdbFile(animTimeStep);
    SdfAssetPath volAsset(volumeCache.TimedRelOvdbFile);

    bool standardVolumeFields = (numUga == 0);

    MANAGE_SET_AND_LINK_VOLUME_FIELD(volumeCache.OvdbDensityFieldPath, OmniConnectTokens->density, OmniConnectTokens->density, standardVolumeFields);
    MANAGE_SET_AND_LINK_VOLUME_FIELD(volumeCache.OvdbDiffuseFieldPath, OmniConnectTokens->diffuse, OmniConnectTokens->diffuse, standardVolumeFields && omniVolumeData.preClassified);
    for(int arrIdx = 0; arrIdx < numUga; ++ arrIdx)
    {
      OmniConnectGenericArray& genArr = updatedGenericArrays[arrIdx];

      volumeCache.ResetTempFieldPathAndTokens(genArr.Name);

      MANAGE_SET_AND_LINK_VOLUME_FIELD(volumeCache.TempFieldPath, volumeCache.TempFieldRelToken, volumeCache.TempFieldGridToken, true);
    }
    for(int arrIdx = 0; arrIdx < numDga; ++ arrIdx)
    {
      OmniConnectGenericArray& genArr = deletedGenericArrays[arrIdx];

      volumeCache.ResetTempFieldPathAndTokens(genArr.Name);
      
      MANAGE_SET_AND_LINK_VOLUME_FIELD(volumeCache.TempFieldPath, volumeCache.TempFieldRelToken, volumeCache.TempFieldGridToken, false);
    }
  }

  // Generic part of update

  SyncTimeVaryingAttribute(geomActPrim.GetExtentAttr(), geomClipPrim.GetExtentAttr(), geomTopPrim,
    timeVaryingUpdate, timeCode, OmniConnectTokens->extent, &UsdGeomGprim::CreateExtentAttr);

  if (performsUpdate)
  {
    // Set extents in usd
    VtVec3fArray extentArray(2);
    extentArray[0].Set(minX, minY, minZ);
    extentArray[1].Set(maxX, maxY, maxZ);

    geomOutPrim.GetExtentAttr().Set(extentArray, timeCode); // Always timevarying

    // Write VDB data 
    VolumeWriter->ToVDB(omniVolumeData,
      updatedGenericArrays, numUga);

    // Get serialized data
    const char* volumeStreamData; size_t volumeStreamDataSize;
    VolumeWriter->GetSerializedVolumeData(volumeStreamData, volumeStreamDataSize);

    // Write to file
    std::string& ovdbFile = volumeCache.TimedOvdbFile;
    bool fileWritten = Connection->WriteFile(volumeStreamData, volumeStreamDataSize, ovdbFile.c_str());
    if(!fileWritten)
    {
      OmniConnectErrorMacro("Cannot write volume file " << ovdbFile.c_str());
    }
  }

  if (UsdSaveEnabled)
  {
    geomTopologyStage->Save();
    geomClipStage->Save();
  }

  return newVolume;
}

void OmniConnectInternals::ManageSetAndLinkOpenVDBAsset(UsdStageRefPtr& actorStage, UsdStageRefPtr& geomClipStage, UsdStageRefPtr& geomTopologyStage, const UsdVolVolume& volActGeom,
  const SdfPath& primPath, const TfToken& relToken, const TfToken& gridToken, bool createField, const SdfAssetPath& volAsset,
  bool performsUpdate, bool timeVaryingUpdate, const UsdTimeCode& timeCode)
{
  // Manage: Create or remove the asset
  UsdVolOpenVDBAsset ovdbDiffuseFieldAct = CreateOrRemoveOpenVDBAsset(actorStage, primPath, gridToken, createField, true);
  UsdVolOpenVDBAsset ovdbDiffuseFieldClip = CreateOrRemoveOpenVDBAsset(geomClipStage, primPath, gridToken, createField, false);
  UsdVolOpenVDBAsset ovdbDiffuseFieldTop = CreateOrRemoveOpenVDBAsset(geomTopologyStage, primPath, gridToken, createField, false);

  // Manage: Sync file path attribute across timevarying/uniform stages
  if(createField)
      SyncTimeVaryingAttribute(ovdbDiffuseFieldAct.GetFilePathAttr(), ovdbDiffuseFieldClip.GetFilePathAttr(), ovdbDiffuseFieldTop,
        timeVaryingUpdate, timeCode, OmniConnectTokens->filePath, &UsdVolOpenVDBAsset::CreateFilePathAttr);
  
  if (performsUpdate)
  {
    // Set: File path attribute
    if (createField)
    {
      UsdVolOpenVDBAsset ovdbDiffuseFieldOut = timeVaryingUpdate ? ovdbDiffuseFieldClip : ovdbDiffuseFieldAct;
      UsdAttribute colFileAttr = ovdbDiffuseFieldOut.GetFilePathAttr();
      colFileAttr.Set(volAsset, timeCode);
    }

    // Link: field relationship based on existence of field (relationships cannot be timevarying, so just perform on top prim)
    if (createField)
      volActGeom.CreateFieldRelationship(relToken, primPath);
    else
    {
      if (volActGeom.HasFieldRelationship(relToken))
        volActGeom.BlockFieldRelationship(relToken);
    }
  }
}

void OmniConnectInternals::RemoveActorGeomUniformData(OmniConnectActorCache & actorCache, OmniConnectMeshCache & meshCache)
{
}

void OmniConnectInternals::RemoveActorGeomUniformData(OmniConnectActorCache & actorCache, OmniConnectInstancerCache & instancerCache)
{
}

void OmniConnectInternals::RemoveActorGeomUniformData(OmniConnectActorCache & actorCache, OmniConnectCurveCache & curveCache)
{
}

void OmniConnectInternals::RemoveActorGeomUniformData(OmniConnectActorCache & actorCache, OmniConnectVolumeCache & volumeCache)
{
}

void OmniConnectInternals::RemoveActorGeomVaryingData(OmniConnectActorCache& actorCache, OmniConnectMeshCache& meshCache, double animTimeStep)
{
}

void OmniConnectInternals::RemoveActorGeomVaryingData(OmniConnectActorCache& actorCache, OmniConnectInstancerCache& instancerCache, double animTimeStep)
{
}

void OmniConnectInternals::RemoveActorGeomVaryingData(OmniConnectActorCache & actorCache, OmniConnectCurveCache & curveCache, double animTimeStep)
{
}

void OmniConnectInternals::RemoveActorGeomVaryingData(OmniConnectActorCache& actorCache, OmniConnectVolumeCache& volumeCache, double animTimeStep)
{
  volumeCache.ResetTimedOvdbFile(animTimeStep);
  std::string& ovdbFile = volumeCache.TimedOvdbFile;

  Connection->RemoveFile(ovdbFile.c_str());
}

void OmniConnectInternals::RemovePrimAndTopology(
  OmniConnectActorCache& actorCache, std::string& geomTopologyFilePath, SdfPath& primPath)
{
  // Delete the meshprim
  actorCache.Stage->RemovePrim(primPath);

#ifdef TOPOLOGY_AS_SUBLAYER
  // Delete the topology sublayer ref
  SdfSubLayerProxy slp = actorCache.Stage->GetRootLayer()->GetSubLayerPaths();
  size_t subLayerIndex = slp.Find(geomTopologyFilePath);
  actorCache.Stage->GetRootLayer()->RemoveSubLayerPath((int)subLayerIndex);
#endif

  // Delete the topology file
  Connection->RemoveFile(geomTopologyFilePath.c_str());
}


template<typename CacheType>
bool OmniConnectInternals::RemoveActorGeomAtTime(OmniConnectActorCache& actorCache, CacheType& geomCache, double animTimeStep)
{
  typedef typename UsdTypeFromCache<CacheType>::UsdPrimType UsdPrimType;
  typedef typename UsdTypeFromCache<CacheType>::AltUsdPrimType AltUsdPrimType;

  UsdTimeCode timeCode(animTimeStep);
  SdfPath& geomPath = geomCache.SdfGeomPath;

  UsdPrim geomPrim;
  if (geomCache.UsesAltUsdPrimType)
  {
    AltUsdPrimType geom = AltUsdPrimType::Get(actorCache.Stage, geomPath);
    assert(geom);
    geomPrim = geom.GetPrim();
  }
  else
  {
    UsdPrimType geom = UsdPrimType::Get(actorCache.Stage, geomPath);
    assert(geom);
    geomPrim = geom.GetPrim();
  }

  // Get the clip and topology paths
  geomCache.ResetTopologyFile(actorCache, this->LiveInterface.GetLiveExtension());
  geomCache.ResetTimedGeomClipFile(animTimeStep, this->LiveInterface.GetLiveExtension());
  std::string& geomClipStagePath = geomCache.TimedGeomClipStagePath;
  std::string& geomTopologyStagePath = geomCache.GeomTopologyStagePath;

  // Clear timestep from actor geom clipsapi
  UsdClipsAPI clipsApi(geomPrim);

  VtVec2dArray& clipActives = geomCache.ClipActives;

  for (int timeIndex = 0; timeIndex < clipActives.size(); ++timeIndex)
  {
    if (clipActives[timeIndex][0] == animTimeStep)
    {
      GfVec2d lastElt = clipActives[clipActives.size() - 1];
      clipActives[timeIndex] = lastElt;
      clipActives.pop_back();
      break;
    }
  }

  // If no timesamples left for the geom, remove the geom from the actor stage, and remove the topology file
  bool geomDeleted = (clipActives.size() == 0);
  if (geomDeleted)
  {
    // Remove uniform geom-specific data
    RemoveActorGeomUniformData(actorCache, geomCache);
    // Remove geom prim and topology sublayer reference+file
    RemovePrimAndTopology(actorCache, geomTopologyStagePath, geomPath);
  }
  else
  {
    clipsApi.SetClipActive(clipActives);
  }

  // Remove both the clip file and time-varying geom-specific data associated with it
  RemoveActorGeomVaryingData(actorCache, geomCache, animTimeStep);
  Connection->RemoveFile(geomClipStagePath.c_str());

  return geomDeleted;
}

template<typename CacheType>
void OmniConnectInternals::RemoveActorGeom(OmniConnectActorCache& actorCache, CacheType& geomCache)
{
  typedef typename UsdTypeFromCache<CacheType>::UsdPrimType UsdPrimType;
  typedef typename UsdTypeFromCache<CacheType>::AltUsdPrimType AltUsdPrimType;

  SdfPath& geomPath = geomCache.SdfGeomPath;
  if (geomCache.UsesAltUsdPrimType)
  {
    AltUsdPrimType geom = AltUsdPrimType::Get(actorCache.Stage, geomPath);
    if (!geom)
      return;
  }
  else
  {
    UsdPrimType geom = UsdPrimType::Get(actorCache.Stage, geomPath);
    if (!geom)
      return;
  }

  // gather geom files for all timesteps
  VtVec2dArray& clipActives = geomCache.ClipActives;
  VtArray<SdfAssetPath>& assetPaths = geomCache.ClipAssetPaths;

  // Remove uniform geom-specific data
  RemoveActorGeomUniformData(actorCache, geomCache);

  // Remove geom prim and topology sublayer reference+file
  geomCache.ResetTopologyFile(actorCache, this->LiveInterface.GetLiveExtension());
  RemovePrimAndTopology(actorCache, geomCache.GeomTopologyStagePath, geomPath);

  // Remove the geometry files for all timesteps
  for (int timeIndex = 0; timeIndex < clipActives.size(); ++timeIndex)
  {
    int clipIndex = int(clipActives[timeIndex][1]);
    std::string geomFilePath = this->SceneDirectory + assetPaths[clipIndex].GetAssetPath();

#ifdef FORCE_OMNI_CLIP_UPDATES_WITH_SUBLAYER
    //Remove assetpath from sublayers
    SdfSubLayerProxy slp = actorCache.Stage->GetRootLayer()->GetSubLayerPaths();
    size_t subLayerIndex = slp.Find(geomFilePath);
    actorCache.Stage->GetRootLayer()->RemoveSubLayerPath(subLayerIndex);
#endif

    // Remove both the clip file and time-varying geom-specific data associated with it
    RemoveActorGeomVaryingData(actorCache, geomCache, clipActives[timeIndex][0]);
    Connection->RemoveFile(geomFilePath.c_str());

    // Don't delete the assetpath (variable), as it will invalidate the clip indices of other geometries
  }
}

template<typename CacheType>
void OmniConnectInternals::RemoveAllActorGeoms(OmniConnectActorCache& actorCache)
{
  std::map<size_t, CacheType>& geomCaches = actorCache.GetGeomCaches<CacheType>();
  for (auto& geomCache : geomCaches)
  {
    RemoveActorGeom<CacheType>(actorCache, geomCache.second);
  }
  geomCaches.clear();
}

void OmniConnectInternals::UpdateTransform(OmniConnectActorCache& actorCache, double* transform, double animTimeStep)
{
  UsdTimeCode timeCode(animTimeStep);

  GfMatrix4d transMat;
  transMat.SetColumn(0, GfVec4d(&transform[0]));
  transMat.SetColumn(1, GfVec4d(&transform[4]));
  transMat.SetColumn(2, GfVec4d(&transform[8]));
  transMat.SetColumn(3, GfVec4d(&transform[12]));

  //Note that group transform nodes have already been created.
  UsdGeomXform tfNodeActor = UsdGeomXform::Get(actorCache.Stage, actorCache.SdfActorPrimPath);
  tfNodeActor.ClearXformOpOrder();
  tfNodeActor.AddTransformOp().Set(transMat, timeCode);
}

template<typename CacheType>
void OmniConnectInternals::SetMaterialBinding(OmniConnectActorCache& actorCache, size_t geomId, size_t matId)
{
  CacheType& geomCache = actorCache.GetGeomCache<CacheType>(geomId).second;

  UsdPrim geomPrim = actorCache.Stage->GetPrimAtPath(geomCache.SdfGeomPath);
  assert(geomPrim);

  OmniConnectMatCache& matCache = actorCache.GetMatCache(matId).second;

  UsdShadeMaterial material = UsdShadeMaterial::Get(actorCache.Stage, matCache.SdfMatName);
  assert(material);

  geomPrim.ApplyAPI<UsdShadeMaterialBindingAPI>();
  UsdShadeMaterialBindingAPI(geomPrim).Bind(material);
}

void OmniConnectInternals::SetTimeStepCodes(UsdStageRefPtr& stage, double newTime)
{
  // Make sure the stage edit target is the root layer
  UsdEditTarget prevEditTarget;
  if(stage->GetRootLayer() != stage->GetEditTarget().GetLayer())
  {
    prevEditTarget = stage->GetEditTarget();
    stage->SetEditTarget(UsdEditTarget(stage->GetRootLayer()));
  }  

  // Set the time codes if they expand the range
  double oldStartTime = stage->GetStartTimeCode();
  double oldEndTime = stage->GetEndTimeCode();
  bool notInitialized = !stage->HasAuthoredTimeCodeRange();

  if (notInitialized || newTime < oldStartTime)
  {
    stage->SetStartTimeCode(newTime);
  }
  if (notInitialized || newTime > oldEndTime)
  {
    stage->SetEndTimeCode(newTime);
  }

  // Set the edit target back to whatever it was
  if(prevEditTarget.IsValid())
  {
    if (!notInitialized && (newTime < oldStartTime || newTime > oldEndTime))
      OmniConnectDebugMacro("Timecode range has been expanded with live workflow enabled. This will likely require a reload of active live sessions in other applications.");

    stage->SetEditTarget(prevEditTarget);
  }
}

UsdPrim OmniConnectInternals::FindOrCreateScenePrim(SdfPath& primPath, UsdStageRefPtr& sceneStage, std::string& clipPath)
{
  // make sure to execute before adding sublayers, otherwise first write detection is not working
  UsdPrim actorScenePrim = sceneStage->GetPrimAtPath(primPath);

  if (!actorScenePrim)
  {
    actorScenePrim = sceneStage->OverridePrim(primPath);

    // Add clip value dict
    UsdClipsAPI clipsApi(actorScenePrim);

    clipsApi.SetClipPrimPath(primPath.GetString());

    VtArray<SdfAssetPath> assetPaths;
    assetPaths.push_back(SdfAssetPath(clipPath));
    clipsApi.SetClipAssetPaths(assetPaths);

    // Set one active clip for the whole timeline
    VtVec2dArray clipActives;
    clipActives.push_back(GfVec2d(0.0, 0.0));
    clipsApi.SetClipActive(clipActives);
  }

  return actorScenePrim;
}

void OmniConnectInternals::DeleteSceneActor(OmniConnectActorCache& actorCache, UsdStageRefPtr& sceneStage)
{
  SdfPath& actorScenePath = actorCache.SdfActorPrimPath;
  sceneStage->RemovePrim(actorScenePath);

//#ifdef TOPOLOGY_AS_SUBLAYER
  SdfSubLayerProxy slp = sceneStage->GetRootLayer()->GetSubLayerPaths();
  size_t subLayerIndex = slp.Find(actorCache.OutputFile);
  sceneStage->GetRootLayer()->RemoveSubLayerPath((int)subLayerIndex);
//#else
  // Remove the reference to the specific actor Material category
  //UsdPrim matScopePrim = sceneStage->GetPrimAtPath(this->SdfMatScopeName);
  //SdfReference ref(actorCache.OutputFile, this->SdfMatScopeName);
  //matScopePrim.GetReferences().RemoveReference(ref);
//#endif
}

void OmniConnectInternals::CreateSceneActor(UsdStageRefPtr& sceneStage, OmniConnectActorCache& actorCache)
{
  // Define both a mesh and instancer prim on the scene stage (make sure to execute before adding sublayers)
  UsdPrim actorPrim = FindOrCreateScenePrim(actorCache.SdfActorPrimPath, sceneStage, actorCache.OutputFile);

//#ifdef TOPOLOGY_AS_SUBLAYER
  // Add the actor output stage as a reference, (it should not yet exist)
  if(sceneStage->GetRootLayer()->GetSubLayerPaths().Find(actorCache.OutputFile) == -1)
    sceneStage->GetRootLayer()->InsertSubLayerPath(actorCache.OutputFile);
//#else
  //actorPrim.GetReferences().AddReference(actorCache.OutputFile, actorCache.SdfActorPrimPath);

  //Also add a reference the Material category of the actor file
  //UsdPrim matScopePrim = sceneStage->GetPrimAtPath(this->SdfMatScopeName);
  //assert(matScopePrim);
  //matScopePrim.GetReferences().AddReference(actorCache.OutputFile, this->SdfMatScopeName);
//#endif

}

size_t OmniConnectInternals::RetrieveSceneToAnimTimesFromUsd(const UsdPrim& actorPrim)
{
  UsdClipsAPI actorClipsApi(actorPrim);
  actorClipsApi.GetClipTimes(&TempSceneToAnimTimes);

  return TempSceneToAnimTimes.size();
}

void OmniConnectInternals::CopySceneToAnimTimes(double* sceneToAnimTimes, size_t numSceneToAnimTimes)
{
  constexpr size_t destEltSize = sizeof(decltype(TempSceneToAnimTimes)::value_type);
  size_t safeSize = std::min( numSceneToAnimTimes*sizeof(double)*2,
    TempSceneToAnimTimes.size() * destEltSize);
  if(safeSize > 0)
    std::memcpy(sceneToAnimTimes, TempSceneToAnimTimes.data(), safeSize);
}

template<typename CacheType>
void OmniConnectInternals::RestoreClipActivesAndPaths(const OmniConnectActorCache& actorCache, CacheType& geomCache)
{
  const SdfPath& geomPath = geomCache.SdfGeomPath;
  UsdPrim actorGeomPrim = actorCache.Stage->GetPrimAtPath(geomPath); // Get geometry prim from the actor stage
  if(actorGeomPrim)
  {
    UsdClipsAPI actorClipsApi(actorGeomPrim);

    actorClipsApi.GetClipAssetPaths(&geomCache.ClipAssetPaths);
    actorClipsApi.GetClipActive(&geomCache.ClipActives);
  }
}

void OmniConnectInternals::RetimeSceneToAnimClips(const VtVec2dArray& sceneClipTimes, const VtVec2dArray& animClipActives, VtVec2dArray& newAnimClipActives)
{
  // Retime according to the old clipactives.  
  // Find for every entry s{stagetime, cliptime} in sceneClipTimes the tuple a{stagetime, assetindex} in animClipActives with s.cliptime == a.stagetime, and take its assetindex.
  // Note that to make sure that for every s{}, there is an a{} such that s.cliptime == a.stagetime, invidual anim geometry should have already been updated accordingly.
  // However, if geometry is not updated due to it being empty or removed for a timestep, we just point to asset 0 and thereby defer to other mechanisms (such as visibility, see actornodebase).
  for (int clipIdx = 0; clipIdx < sceneClipTimes.size(); ++clipIdx)
  {
    double clipTime = sceneClipTimes[clipIdx][1];
    double assetIndex = 0; 
    for (int activeIdx = 0; activeIdx < animClipActives.size(); ++activeIdx)
    {
      if (clipTime == animClipActives[activeIdx][0])
        assetIndex = animClipActives[activeIdx][1];
    }
    newAnimClipActives[clipIdx] = GfVec2d(sceneClipTimes[clipIdx][0], assetIndex);
  }
}

template<typename CacheType>
void OmniConnectInternals::RetimeActorGeoms(OmniConnectActorCache& actorCache, UsdStageRefPtr& scenestage, const VtVec2dArray& sceneClipTimes, VtVec2dArray& newAnimClipActives)
{
  typedef typename UsdTypeFromCache<CacheType>::UsdPrimType UsdPrimType;
  typedef typename UsdTypeFromCache<CacheType>::AltUsdPrimType AltUsdPrimType;

  std::map<size_t, CacheType>& geomCaches = actorCache.GetGeomCaches<CacheType>();

  for (auto& geomCachePair : geomCaches)
  {
    auto& geomCache = geomCachePair.second;

    UsdPrim geomPrim;
    if (geomCache.UsesAltUsdPrimType)
    {
      AltUsdPrimType geom = AltUsdPrimType::Get(scenestage, geomCache.SdfGeomPath);
      assert(geom);
      geomPrim = geom.GetPrim();
    }
    else
    {
      UsdPrimType geom = UsdPrimType::Get(scenestage, geomCache.SdfGeomPath);
      assert(geom);
      geomPrim = geom.GetPrim();
    }

    VtVec2dArray& animClipActives = geomCache.ClipActives;
    RetimeSceneToAnimClips(sceneClipTimes, animClipActives, newAnimClipActives);

    UsdClipsAPI geomClipsApi(geomPrim);
    geomClipsApi.SetClipActive(newAnimClipActives);
    geomClipsApi.SetClipTimes(sceneClipTimes);
  }
}

void OmniConnectInternals::SetClipValues(OmniConnectActorCache& actorCache, UsdStageRefPtr& scenestage, UsdPrim& actorPrim, 
  const double* sceneToAnimTimes, size_t numSceneToAnimTimes)
{
  //WITHIN THE SCENE STAGE ONLY; defines a value clip around mesh/instancer GROUPS, in combination with overriding the "actives" attribute of the individual mesh/instancer PRIM value clips,
  //to enforce a different timing according to "sceneToAnimTimes" for a particular ACTOR. Value clips within the ACTOR STAGE are left unchanged, 
  //and only used for reading actor-space timing information, required for composing the scene-based retiming in the SCENE STAGE.

  const GfVec2d* gfSceneAnimTimes = reinterpret_cast<const GfVec2d*>(sceneToAnimTimes);
  VtVec2dArray& sceneClipTimes = actorCache.TempClipTimes;
  sceneClipTimes.assign(gfSceneAnimTimes, gfSceneAnimTimes + numSceneToAnimTimes);

  // Set timing for mesh/instancer groups in the scene stage for this actor. Only one clip is active at all times, which has already been set; 
  // the retiming takes care of all the NON-mesh/instancer attributes in that clip.  
  UsdClipsAPI actorClipsApi(actorPrim);
  actorClipsApi.SetClipTimes(sceneClipTimes);

  // The value clips for the individual meshes/instancers has to be OVERRIDDEN in the scene stage, 
  // with the FLATTENED values of the scenetime->assetindex evaluation.
  // This is because the individual geoms define the lowest-level value clip (the first parent) of the geometry, 
  // which fully determines the retiming.
  // So the "times" in the value clip are not touched, but the "actives" array is simply overridden from actortime->assetindex 
  // in the actor stage to scenetime->actortime->assetindex = scenetime->assetindex in the scene stage.
  // Given a scenetime->actortime pair, one requirement is that the mesh/instancer clip information at actortime 
  // has already been added to the actor stage.
  VtVec2dArray& newAnimClipActives = actorCache.TempNewClipActives;
  newAnimClipActives.resize(sceneClipTimes.size());
  
  RetimeActorGeoms<OmniConnectMeshCache>(actorCache, scenestage, sceneClipTimes, newAnimClipActives);
  RetimeActorGeoms<OmniConnectInstancerCache>(actorCache, scenestage, sceneClipTimes, newAnimClipActives);
  RetimeActorGeoms<OmniConnectCurveCache>(actorCache, scenestage, sceneClipTimes, newAnimClipActives);
  RetimeActorGeoms<OmniConnectVolumeCache>(actorCache, scenestage, sceneClipTimes, newAnimClipActives);
}

#ifdef FORCE_OMNI_CLIP_UPDATES_WITH_DUMMY
void OmniConnectInternals::AttachAsSublayer(UsdStageRefPtr& stage, std::string& fileName, bool attach)
{
  if (!attach)
  {
    SdfSubLayerProxy slp = stage->GetRootLayer()->GetSubLayerPaths();
    size_t subLayerIndex = slp.Find(fileName);
    if(subLayerIndex != size_t(-1))
      stage->GetRootLayer()->RemoveSubLayerPath((int)subLayerIndex);
  }
  else
  {
    stage->GetRootLayer()->InsertSubLayerPath(fileName);
  }
}
#endif

void OmniConnectInternals::SetActorVisibility(OmniConnectActorCache& actorCache, bool visible, double animTimeStep)
{
  UsdGeomXform actorPrim = UsdGeomXform::Get(actorCache.Stage, actorCache.SdfActorPrimPath);

  UsdAttribute meshVisAttrib = actorPrim.GetVisibilityAttr();
  if (!meshVisAttrib)
  {
    meshVisAttrib = actorPrim.CreateVisibilityAttr();
  }
  
  TfToken visibleToken = visible ? UsdGeomTokens->inherited : UsdGeomTokens->invisible;
  UsdTimeCode timeCode = animTimeStep == -1 ? UsdTimeCode::Default() : UsdTimeCode(animTimeStep);
  meshVisAttrib.Set(visibleToken, timeCode);
}

template<typename CacheType>
void OmniConnectInternals::SetGeomVisibility(OmniConnectActorCache& actorCache, size_t geomId, bool visible, double animTimeStep)
{
  typedef typename UsdTypeFromCache<CacheType>::UsdPrimType UsdPrimType;
  typedef typename UsdTypeFromCache<CacheType>::AltUsdPrimType AltUsdPrimType;

  CacheType& geomCache = actorCache.GetGeomCache<CacheType>(geomId).second;

  SdfPath& geomPath = geomCache.SdfGeomPath;
  UsdGeomImageable geomIm;
  if (geomCache.UsesAltUsdPrimType)
  {
    AltUsdPrimType geom = AltUsdPrimType::Get(actorCache.Stage, geomPath);
    assert(geom);
    geomIm = geom;
  }
  else
  {
    UsdPrimType geom = UsdPrimType::Get(actorCache.Stage, geomPath);
    assert(geom);
    geomIm = geom;
  }

  UsdAttribute geomVisAttrib = geomIm.GetVisibilityAttr();
  TfToken visibleToken = visible ? UsdGeomTokens->inherited : UsdGeomTokens->invisible;
  UsdTimeCode timeCode = animTimeStep == -1 ? UsdTimeCode::Default() : UsdTimeCode(animTimeStep);
  geomVisAttrib.Set(visibleToken, timeCode);
}

void OmniConnectInternals::SetLiveWorkflowEnabled(bool enable)
{
  if(enable)
  {
    if(this->Settings.OutputLocal)
    {
      OmniConnectErrorMacro("Live workflows can only be enabled for output on an Omniverse Nucleus server.");
      return;
    }
    if(!OmniConnectRemoteConnection::OmniClientInitialized)
    {
      OmniConnectErrorMacro("Live workflows can only be enabled when an Omniverse client connection with a Nucleus server has been established.");
      return;
    }
  }

  LiveInterface.SetLiveWorkflow(enable,
    this->MultiSceneStage, this->SceneStage,
    this->MultiSceneStageUrl, this->SceneStageUrl,
    this->MultiSceneFileName, this->SceneFileName,
    &this->ActorCacheMap);
}

#define LIVE_WORKFLOW_DISABLED_SCOPE OmniConnectInternals::ForceLiveWorkflowDisabled lwfDisabled(Internals)

OmniConnect::OmniConnect(const OmniConnectSettings& settings
  , const OmniConnectEnvironment& environment
  , OmniConnectLogCallback logCallback
)
{
  if (settings.OutputLocal)
    Connection = new OmniConnectLocalConnection();
  else
    Connection = new OmniConnectRemoteConnection();
  
  Internals = new OmniConnectInternals(settings, environment, Connection, logCallback);
}

const OmniConnectSettings& OmniConnect::GetSettings()
{
  return Internals->Settings;
}

bool OmniConnect::OpenConnection(bool createSession)
{
  std::string outputDir = Internals->Settings.OutputLocal ? Internals->Settings.LocalOutputDirectory : Internals->Settings.OmniWorkingDirectory;
  FormatDirName(outputDir);
  OmniConnectConnectionSettings connSettings = { Internals->Settings.OmniServer, outputDir, createSession };
  ConnectionValid = Connection->Initialize(connSettings, OmniConnectInternals::LogCallback, nullptr);
  if (ConnectionValid && createSession)
  {
    Internals->InitializeSession();
    Internals->OpenSceneStage();
    if (Internals->Environment.ProcId == 0 && Internals->Environment.NumProcs > 1)
      Internals->OpenMultiSceneStage();
    Internals->OpenRootLevelStage();
  }
  return ConnectionValid;
}

void OmniConnect::CloseConnection()
{
  SetLiveWorkflowEnabled(false);

  Internals->SessionNumber = -1;

  if (ConnectionValid)
  {
    Connection->Shutdown();
    ConnectionValid = false;
  }
}

const char* OmniConnect::GetUser(const char* serverUrl) const
{
  return this->Connection->GetOmniUser(serverUrl);
}

OmniConnectUrlInfoList OmniConnect::GetUrlInfoList(const char* serverUrl) const
{
  return this->Connection->GetOmniConnectUrlInfoList(serverUrl);
}

bool OmniConnect::CreateFolder(const char* url)
{
  if (this->ConnectionValid) 
  {
    return this->Connection->CreateFolder(url, true, false);
  }
  return false;
}

int OmniConnect::GetLatestSessionNumber() const {
  return this->Internals->FindSessionNumber();
}

const char* OmniConnect::GetClientVersion() const {
  return this->Connection->GetOmniClientVersion();
}

void OmniConnect::SetAuthMessageBoxCallback(void* userData, OmniConnectAuthCallback callback) {
  this->Connection->SetAuthMessageBoxCallback(userData, callback);
}

void OmniConnect::CancelOmniClientAuth(uint32_t authHandle) 
{
  this->Connection->CancelOmniClientAuth(authHandle);
}

bool OmniConnect::OmniClientEnabled() {
#ifdef OMNIVERSE_CONNECTION_ENABLE
	return true;
#else
	return false;
#endif
}

OmniConnect::~OmniConnect()
{
  CloseConnection();
  
  delete Internals;
  delete Connection;
}

bool OmniConnect::CreateActor(size_t actorId, const char* actorName)
{
  auto itSuccessPair = Internals->ActorCacheMap.emplace(actorId, OmniConnectActorCache());
  OmniConnectActorCache& actorCache = (*itSuccessPair.first).second;
  if (itSuccessPair.second)
  {
    LIVE_WORKFLOW_DISABLED_SCOPE;

    actorCache.SetPathsAndNames(actorId, actorName, Internals->SceneDirectory, 
      Internals->RootPrimName, Internals->ActScopeName, Internals->MatScopeName, Internals->TexScopeName,
      Internals->UsdExtension.c_str(), Internals->Environment);

    // Create the actor usd itself
    Internals->OpenActorStage(actorCache);

    // Create the prim within the Scene usd referencing the actor usd.
    Internals->CreateSceneActor(Internals->SceneStage, actorCache);

    return true;
  }
  return false;
}

void OmniConnect::DeleteActor(size_t actorId)
{
  LIVE_WORKFLOW_DISABLED_SCOPE;

  auto cacheIt = Internals->ActorCacheMap.find(actorId);

  // Remove actor prim/reference from scene
  DeleteActorFromScene(actorId);

  // Remove the actor usd itself, with associated materials/textures/geometry
  if (cacheIt != Internals->ActorCacheMap.end())
  {
    OmniConnectActorCache& actorCache = cacheIt->second;

    //Remove materials
    Internals->TempIdVec.resize(0);
    UsdPrim matScopePrim = actorCache.Stage->GetPrimAtPath(Internals->SdfMatScopeName);
    if (matScopePrim)
    {
      UsdPrimSiblingRange matPrims = matScopePrim.GetAllChildren();
      for (UsdPrim matPrim : matPrims)
      {
        size_t matId;
        matPrim.GetAttribute(OmniConnectTokens->MatId).Get(&matId);
        Internals->TempIdVec.push_back(matId);
      }
      for (size_t matId : Internals->TempIdVec)
      {
        Internals->RemoveActorMaterial(actorCache, matId);
      }
    }

    // Remove Textures
    for (int i = 0; i < actorCache.TexCaches.size(); ++i)
      Internals->DeleteTexture(actorCache, i);
    actorCache.TexCaches.clear();

    // Remove Geometry
    Internals->RemoveAllActorGeoms<OmniConnectMeshCache>(actorCache);
    Internals->RemoveAllActorGeoms<OmniConnectInstancerCache>(actorCache);
    Internals->RemoveAllActorGeoms<OmniConnectCurveCache>(actorCache);
    Internals->RemoveAllActorGeoms<OmniConnectVolumeCache>(actorCache);

    std::string usdFileToRemove(std::move(actorCache.UsdOutputFilePath)); 
    //SdfLayerHandle rootLayer = actorCache.Stage->GetRootLayer();
    //rootLayer->Clear(); //No way to explicitly destroy the layer, so we just clear and handle reopening in OpenActorStage.
    //rootLayer->Save();

    actorCache.Stage.Reset(); // Clean up the stage refptr manually (maybe not required due to next step)
    Internals->ActorCacheMap.erase(cacheIt); // erase the stage pointer before removing the physical file    

    //Explicitly remove usd file, but only do this in case a stage can truly be unloaded (see CreateNew() in OpenActorStage)
    Connection->RemoveFile(usdFileToRemove.c_str());
  }
}

void OmniConnect::SetActorVisibility(size_t actorId, bool visible, double animTimeStep)
{
  OmniConnectActorCache* actorCache = this->Internals->GetCachedActorCache(actorId);

  this->Internals->SetActorVisibility(*actorCache, visible, animTimeStep);
}

void OmniConnect::SetGeomVisibility(size_t actorId, size_t geomId, OmniConnectGeomType geomType, bool visible, double animTimeStep)
{
  OmniConnectActorCache* actorCache = this->Internals->GetCachedActorCache(actorId);

  switch (geomType)
  {
  case OmniConnectGeomType::MESH:
    this->Internals->SetGeomVisibility<OmniConnectMeshCache>(*actorCache, geomId, visible, animTimeStep);
    break;
  case OmniConnectGeomType::INSTANCER:
    this->Internals->SetGeomVisibility<OmniConnectInstancerCache>(*actorCache, geomId, visible, animTimeStep);
    break;
  case OmniConnectGeomType::CURVE:
    this->Internals->SetGeomVisibility<OmniConnectCurveCache>(*actorCache, geomId, visible, animTimeStep);
    break;
  case OmniConnectGeomType::VOLUME:
    this->Internals->SetGeomVisibility<OmniConnectVolumeCache>(*actorCache, geomId, visible, animTimeStep);
    break;
  default:
    assert(false);
    break;
  }
}

void OmniConnect::AddTimeStep(size_t actorId, double animTimeStep)
{
  OmniConnectActorCache* actorCache = this->Internals->GetCachedActorCache(actorId);

  this->Internals->SetTimeStepCodes(actorCache->Stage, animTimeStep);
}

void OmniConnect::SetActorTransform(size_t actorId, double animTimeStep, double* transform)
{
  OmniConnectActorCache* actorCache = this->Internals->GetCachedActorCache(actorId);
  assert(transform);

  this->Internals->UpdateTransform(*actorCache, transform, animTimeStep);
}

void OmniConnect::UpdateMaterial(size_t actorId, const OmniConnectMaterialData& omniBaseMatData, double animTimeStep)
{
  OmniConnectActorCache* actorCache = this->Internals->GetCachedActorCache(actorId);

  this->Internals->UpdateActorMaterial(*actorCache, omniBaseMatData, animTimeStep);
}

void OmniConnect::DeleteMaterial(size_t actorId, size_t materialId)
{
  OmniConnectActorCache* actorCache = this->Internals->GetCachedActorCache(actorId);

  this->Internals->RemoveActorMaterial(*actorCache, materialId);
}

void OmniConnect::UpdateTexture(size_t actorId, size_t texId, OmniConnectSamplerData& samplerData, bool timeVarying, double animTimeStep)
{
  OmniConnectActorCache* actorCache = this->Internals->GetCachedActorCache(actorId);

  this->Internals->UpdateTexture(*actorCache, texId, samplerData, timeVarying, animTimeStep);
}

void OmniConnect::DeleteTexture(size_t actorId, size_t texId)
{
  OmniConnectActorCache* actorCache = this->Internals->GetCachedActorCache(actorId);

  auto pred = [texId](const std::pair<size_t, OmniConnectTexCache>& entry) -> bool { return entry.first == texId; };
  auto it = std::find_if(actorCache->TexCaches.begin(), actorCache->TexCaches.end(), pred);
  if (it == actorCache->TexCaches.end())
    return;

  this->Internals->DeleteTexture(*actorCache, texId);

  *it = std::move(actorCache->TexCaches.back());
  actorCache->TexCaches.pop_back();
}

void OmniConnect::UpdateMesh(size_t actorId, double animTimeStep, OmniConnectMeshData& meshData, size_t materialId, 
  OmniConnectGenericArray* updatedGenericArrays, size_t numUga, OmniConnectGenericArray* deletedGenericArrays, size_t numDga)
{
  OmniConnectActorCache* actorCache = this->Internals->GetCachedActorCache(actorId);

  this->Internals->UpdateGeom<OmniConnectMeshCache>(actorCache, animTimeStep, meshData, meshData.MeshId, materialId, 
    updatedGenericArrays, numUga, deletedGenericArrays, numDga);
}

void OmniConnect::UpdateInstancer(size_t actorId, double animTimeStep, OmniConnectInstancerData& instancerData, size_t materialId, 
  OmniConnectGenericArray* updatedGenericArrays, size_t numUga, OmniConnectGenericArray* deletedGenericArrays, size_t numDga)
{
  OmniConnectActorCache* actorCache = this->Internals->GetCachedActorCache(actorId);

  this->Internals->UpdateGeom<OmniConnectInstancerCache>(actorCache, animTimeStep, instancerData, instancerData.InstancerId, materialId,
    updatedGenericArrays, numUga, deletedGenericArrays, numDga);
}

void OmniConnect::UpdateCurve(size_t actorId, double animTimeStep, OmniConnectCurveData & curveData, size_t materialId, 
  OmniConnectGenericArray * updatedGenericArrays, size_t numUga, OmniConnectGenericArray * deletedGenericArrays, size_t numDga)
{
  OmniConnectActorCache* actorCache = this->Internals->GetCachedActorCache(actorId);

  this->Internals->UpdateGeom<OmniConnectCurveCache>(actorCache, animTimeStep, curveData, curveData.CurveId, materialId,
    updatedGenericArrays, numUga, deletedGenericArrays, numDga);
}

void OmniConnect::UpdateVolume(size_t actorId, double animTimeStep, OmniConnectVolumeData & volumeData, size_t materialId, 
  OmniConnectGenericArray * updatedGenericArrays, size_t numUga, OmniConnectGenericArray* deletedGenericArrays, size_t numDga)
{
  OmniConnectActorCache* actorCache = this->Internals->GetCachedActorCache(actorId);

  this->Internals->UpdateGeom<OmniConnectVolumeCache>(actorCache, animTimeStep, volumeData, volumeData.VolumeId, materialId,
    updatedGenericArrays, numUga, deletedGenericArrays, numDga);
}

bool OmniConnect::DeleteGeomAtTime(size_t actorId, double animTimeStep, size_t geomId, OmniConnectGeomType geomType)
{
  OmniConnectActorCache* actorCache = this->Internals->GetCachedActorCache(actorId);

  bool geomDeleted = false;
  switch (geomType)
  {
  case OmniConnectGeomType::MESH:
    geomDeleted = this->Internals->RemoveGeomAtTime<OmniConnectMeshCache>(actorCache, animTimeStep, geomId);
    break;
  case OmniConnectGeomType::INSTANCER:
    geomDeleted = this->Internals->RemoveGeomAtTime<OmniConnectInstancerCache>(actorCache, animTimeStep, geomId);
    break;
  case OmniConnectGeomType::CURVE:
    geomDeleted = this->Internals->RemoveGeomAtTime<OmniConnectCurveCache>(actorCache, animTimeStep, geomId);
    break;
  case OmniConnectGeomType::VOLUME:
    geomDeleted = this->Internals->RemoveGeomAtTime<OmniConnectVolumeCache>(actorCache, animTimeStep, geomId);
    break;
  default:
    assert(false);
    break;
  }

  return geomDeleted;
}

void OmniConnect::DeleteGeom(size_t actorId, size_t geomId, OmniConnectGeomType geomType)
{
  OmniConnectActorCache* actorCache = this->Internals->GetCachedActorCache(actorId);

  switch (geomType)
  {
  case OmniConnectGeomType::MESH:
    this->Internals->RemoveGeom<OmniConnectMeshCache>(actorCache, geomId);
    break;
  case OmniConnectGeomType::INSTANCER:
    this->Internals->RemoveGeom<OmniConnectInstancerCache>(actorCache, geomId);
    break;
  case OmniConnectGeomType::CURVE:
    this->Internals->RemoveGeom<OmniConnectCurveCache>(actorCache, geomId);
    break;
  case OmniConnectGeomType::VOLUME:
    this->Internals->RemoveGeom<OmniConnectVolumeCache>(actorCache, geomId);
    break;
  default:
    assert(false);
    break;
  }
}

void OmniConnect::SetMaterialBinding(size_t actorId, size_t geomId, size_t materialId, OmniConnectGeomType geomType)
{
  OmniConnectActorCache* actorCache = this->Internals->GetCachedActorCache(actorId);

  switch (geomType)
  {
  case OmniConnectGeomType::MESH:
    this->Internals->SetMaterialBinding<OmniConnectMeshCache>(*actorCache, geomId, materialId);
    break;
  case OmniConnectGeomType::INSTANCER:
    this->Internals->SetMaterialBinding<OmniConnectInstancerCache>(*actorCache, geomId, materialId);
    break;
  case OmniConnectGeomType::CURVE:
    this->Internals->SetMaterialBinding<OmniConnectCurveCache>(*actorCache, geomId, materialId);
    break;
  case OmniConnectGeomType::VOLUME:
    this->Internals->SetMaterialBinding<OmniConnectVolumeCache>(*actorCache, geomId, materialId);
    break;
  default:
    assert(false);
    break;
  }
}

size_t OmniConnect::GetNumSceneToAnimTimes(size_t actorId)
{
  auto it = Internals->ActorCacheMap.find(actorId);
  assert(it != Internals->ActorCacheMap.end());
  OmniConnectActorCache& actorCache = it->second;

  UsdPrim actorPrim = Internals->SceneStage->GetPrimAtPath(actorCache.SdfActorPrimPath);
  assert(actorPrim);

  return Internals->RetrieveSceneToAnimTimesFromUsd(actorPrim);
}
  
void OmniConnect::RestoreSceneToAnimTimes(double* sceneToAnimTimes, size_t numSceneToAnimTimes)
{
  Internals->CopySceneToAnimTimes(sceneToAnimTimes, numSceneToAnimTimes);
}

void OmniConnect::SetSceneToAnimTime(size_t actorId, double sceneTime, const double* sceneToAnimTimes, size_t numSceneToAnimTimes)
{
  //Uses scene times to actor anim times to retime actors within a scene, but ONLY makes changes to the scene stage.

  auto it = Internals->ActorCacheMap.find(actorId);
  assert(it != Internals->ActorCacheMap.end());
  OmniConnectActorCache& actorCache = it->second;

  if(Internals->MultiSceneStage)
    Internals->SetTimeStepCodes(Internals->MultiSceneStage, sceneTime);
  Internals->SetTimeStepCodes(Internals->SceneStage, sceneTime);

  UsdPrim actorPrim = Internals->SceneStage->GetPrimAtPath(actorCache.SdfActorPrimPath);
  assert(actorPrim);

  Internals->SetClipValues(actorCache, Internals->SceneStage, actorPrim, sceneToAnimTimes, numSceneToAnimTimes);

  if (UsdSaveEnabled)
  {
    Internals->SceneStage->Save();
  }
}

void OmniConnect::FlushActorUpdates(size_t actorId)
{
  OmniConnectActorCache* actorCache = this->Internals->GetCachedActorCache(actorId);

  if (UsdSaveEnabled)
  {
    actorCache->Stage->Save();
  }
}


void OmniConnect::FlushSceneUpdates()
{
  if (UsdSaveEnabled)
  {
#ifdef FORCE_OMNI_CLIP_UPDATES_WITH_DUMMY
    int prevDummyIdx = Internals->RunningDummyIndex;
    int newDummyIdx = (prevDummyIdx + 1) % Internals->NumDummyStages;
    Internals->AttachAsSublayer(Internals->SceneStage, Internals->DummyFiles[newDummyIdx], true);
    if (prevDummyIdx != -1)
      Internals->AttachAsSublayer(Internals->SceneStage, Internals->DummyFiles[prevDummyIdx], false);
    Internals->RunningDummyIndex = newDummyIdx;
#endif

    Internals->SceneStage->Save();

    this->Connection->ProcessUpdates();
  }
}

void OmniConnect::SetConnectionLogLevel(int logLevel)
{
  OmniConnectRemoteConnection::SetConnectionLogLevel(logLevel);
}

void OmniConnect::DeleteActorFromScene(size_t actorId)
{
  // Find the actor cache
  auto it = Internals->ActorCacheMap.find(actorId);
  assert(it != Internals->ActorCacheMap.end());
  OmniConnectActorCache& actorCache = it->second;

  // Delete both the prim and the sublayer reference
  Internals->DeleteSceneActor(actorCache, Internals->SceneStage);

  if (UsdSaveEnabled)
  {
    Internals->SceneStage->Save();
  }
}

void OmniConnect::SetUpdateOmniContents(bool update)
{
  if (!ConnectionValid || UsdSaveEnabled == update)
    return;

  // In case the results should be synchronized with the omniverse again, 
  // flush contents of all actor stages created so far and the session stage to omniverse.
  if (update)
  {
    for (auto& mapEntry : Internals->ActorCacheMap)
    {
      mapEntry.second.Stage->Save();
    }
    Internals->SceneStage->Save();

    Connection->ProcessUpdates();
  }
  Internals->UsdSaveEnabled = UsdSaveEnabled = update;
}

void OmniConnect::SetConvertGenericArraysDoubleToFloat(bool convert) 
{ 
  Internals->ConvertGenericArraysDoubleToFloat = convert; 
  Internals->VolumeWriter->SetConvertDoubleToFloat(convert);
}

void OmniConnect::SetLiveWorkflowEnabled(bool enable)
{
  Internals->SetLiveWorkflowEnabled(enable);
}

bool OmniConnect::GetAndResetGeomTypeChanged(size_t actorId)
{
  bool result = false;
  auto cacheIt = Internals->ActorCacheMap.find(actorId);
  if(cacheIt != Internals->ActorCacheMap.end())
  {
    result = cacheIt->second.GeomTypeChanged;
    cacheIt->second.GeomTypeChanged = false;
  }
  return result;
}
