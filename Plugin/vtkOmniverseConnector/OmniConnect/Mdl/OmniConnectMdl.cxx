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

#include "OmniConnectMdl.h"

#ifdef USE_MDL_MATERIALS

#include "OmniConnectConnection.h"
#include "OmniConnectMdlStrings.h"
#include "OmniConnectCaches.h"
#include "OmniConnectUsdUtils.h"
#include "OmniConnectUtilsInternal.h"

#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>

#ifdef _WIN32
#include <filesystem>
namespace fs = std::filesystem;
#else
#if __GNUC__ < 8
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif
#endif

namespace constring
{
  const char* volumeDensityMdl = "OmniVolumeDensity.mdl";
  const char* mdlSupportAssetName = "nvidia/support_definitions.mdl";
  const char* mdlAuxAssetName = "nvidia/aux_definitions.mdl";
#if USE_CUSTOM_MDL || USE_CUSTOM_POINT_SHADER
  const char* opaqueMdl = "PBR_Opaque.mdl";
  const char* translucentMdl = "PBR_Translucent.mdl";
#endif
#if !USE_CUSTOM_MDL
  const char* omniPbrMdl = "OmniPBR.mdl";
#endif
}

namespace
{
  size_t FindLength(std::stringstream& strStream)
  {
    strStream.seekg(0, std::ios::end);
    size_t contentSize = strStream.tellg();
    strStream.seekg(0, std::ios::beg);
    return contentSize;
  }

  void WriteMdlFromStrings(OmniConnectConnection* OmniConnect, const char* string0, const char* string1, const char* fileName)
  {
    size_t strLen0 = std::strlen(string0);
    size_t strLen1 = std::strlen(string1);
    size_t totalStrLen = strLen0 + strLen1;
    char* Mdl_Contents = new char[totalStrLen];
    std::memcpy(Mdl_Contents, string0, strLen0);
    std::memcpy(Mdl_Contents + strLen0, string1, strLen1);

    OmniConnect->WriteFile(Mdl_Contents, totalStrLen, fileName, false);

    delete[] Mdl_Contents;
  }

#if !USE_CUSTOM_MDL
  UsdShadeOutput InitializeUsdMdlGraphNode(UsdShadeShader& graphNode, const TfToken& readerId, const SdfValueTypeName& returnType, const char* sourceAssetPath = constring::mdlSupportAssetName)
  {
    graphNode.CreateImplementationSourceAttr(VtValue(OmniConnectTokens->sourceAsset));
    graphNode.SetSourceAsset(SdfAssetPath(sourceAssetPath), OmniConnectTokens->mdl);
    graphNode.SetSourceAssetSubIdentifier(readerId, OmniConnectTokens->mdl);
    return graphNode.CreateOutput(OmniConnectTokens->out, returnType);
  }
#endif
}

void CreateMdlTemplates(OmniConnectConnection* OmniConnect, OmniConnectMdlNames& mdlNames, const std::string& sceneDirectory, const std::string& materialPath)
{
  mdlNames.MeshVolumeMdl = SdfAssetPath(constring::volumeDensityMdl);

#if USE_CUSTOM_MDL || USE_CUSTOM_POINT_SHADER
  std::string relBase("./" + materialPath);
  std::string opaqueMdl = relBase + std::string(constring::opaqueMdl);
  std::string translucentMdl = relBase + std::string(constring::translucentMdl);

  mdlNames.OpaqueMdl = SdfAssetPath(opaqueMdl);
  mdlNames.TranslucentMdl = SdfAssetPath(translucentMdl);

  WriteMdlFromStrings(OmniConnect, Mdl_PBRBase_string, Mdl_PBRBase_string_opaque, (sceneDirectory + opaqueMdl).c_str());
  WriteMdlFromStrings(OmniConnect, Mdl_PBRBase_string, Mdl_PBRBase_string_translucent, (sceneDirectory + translucentMdl).c_str());
#endif

#if !USE_CUSTOM_MDL
  mdlNames.PbrMdl = SdfAssetPath(constring::omniPbrMdl);
#endif
}

UsdShadeOutput CreateUsdMdlSurfaceShader(const OmniConnectActorCache& actorCache, OmniConnectMatCache& matCache, UsdShadeShader& usdShader, OmniConnectMdlNames& mdlNames, bool newMat
#if USE_CUSTOM_POINT_SHADER
  , bool createCustomShader
#endif
  )
{
  // Could be timevarying
  usdShader.CreateInput(OmniConnectTokens->diffuse_color_constant, SdfValueTypeNames->Color3f);
  usdShader.CreateInput(OmniConnectTokens->emissive_color, SdfValueTypeNames->Color3f);
  usdShader.CreateInput(OmniConnectTokens->emissive_intensity, SdfValueTypeNames->Float);
  usdShader.CreateInput(OmniConnectTokens->enable_emission, SdfValueTypeNames->Bool);
  usdShader.CreateInput(OmniConnectTokens->opacity_constant, SdfValueTypeNames->Float);
  usdShader.CreateInput(OmniConnectTokens->reflection_roughness_constant, SdfValueTypeNames->Float);
  usdShader.CreateInput(OmniConnectTokens->metallic_constant, SdfValueTypeNames->Float);

  // Likely constant
  usdShader.CreateImplementationSourceAttr(VtValue(OmniConnectTokens->sourceAsset));

  UsdShadeOutput output;

#if USE_CUSTOM_POINT_SHADER
  if(createCustomShader)
#endif
#if USE_CUSTOM_MDL || USE_CUSTOM_POINT_SHADER
  {
    usdShader.SetSourceAssetSubIdentifier(OmniConnectTokens->PBR_Base, OmniConnectTokens->mdl);

    usdShader.CreateInput(OmniConnectTokens->ior_constant, SdfValueTypeNames->Float);
    usdShader.CreateInput(OmniConnectTokens->vertexcolor_coordinate_index, SdfValueTypeNames->Int);
    usdShader.CreateInput(OmniConnectTokens->diffuse_wrapmode_u, SdfValueTypeNames->Int);
    usdShader.CreateInput(OmniConnectTokens->diffuse_wrapmode_v, SdfValueTypeNames->Int);

    output = usdShader.CreateOutput(OmniConnectTokens->surface, SdfValueTypeNames->Token);
  }
#endif
#if USE_CUSTOM_POINT_SHADER
  else 
#endif
#if !USE_CUSTOM_MDL
  {
    usdShader.SetSourceAsset(mdlNames.PbrMdl, OmniConnectTokens->mdl);
    usdShader.SetSourceAssetSubIdentifier(OmniConnectTokens->OmniPBR, OmniConnectTokens->mdl);

    usdShader.CreateInput(OmniConnectTokens->enable_opacity, SdfValueTypeNames->Bool);

    {
      // Create a vertexcolorreader
      UsdShadeShader vertexColorReader = UsdDefineOrGet<UsdShadeShader>(actorCache.Stage, matCache.VertexColorReaderPath_mdl, newMat);
      assert(vertexColorReader);
      InitializeUsdMdlGraphNode(vertexColorReader, OmniConnectTokens->data_lookup_color, SdfValueTypeNames->Color3f);
      vertexColorReader.CreateInput(OmniConnectTokens->name, SdfValueTypeNames->String).Set(OmniConnectTokens->displayColor.GetString()); // Set fixed to displaycolor

      // Create a vertexopacityreader
      UsdShadeShader vertexOpacityReader = UsdDefineOrGet<UsdShadeShader>(actorCache.Stage, matCache.VertexOpacityReaderPath_mdl, newMat);
      assert(vertexOpacityReader);
      UsdShadeOutput opacityOutput = InitializeUsdMdlGraphNode(vertexOpacityReader, OmniConnectTokens->data_lookup_float, SdfValueTypeNames->Float);
      vertexOpacityReader.CreateInput(OmniConnectTokens->name, SdfValueTypeNames->String).Set(OmniConnectTokens->displayOpacity.GetString()); // Set fixed to displayopacity

      UsdShadeShader vertexOpacityPow = UsdDefineOrGet<UsdShadeShader>(actorCache.Stage, matCache.VertexOpacityPowPath_mdl, newMat);
      assert(vertexOpacityPow);
      InitializeUsdMdlGraphNode(vertexOpacityPow, OmniConnectTokens->pow_float, SdfValueTypeNames->Float);
      vertexOpacityPow.CreateInput(OmniConnectTokens->a, SdfValueTypeNames->Float).ConnectToSource(opacityOutput);
      vertexOpacityPow.CreateInput(OmniConnectTokens->b, SdfValueTypeNames->Float).Set(1.0f);

      // Create a standard texture coordinate reader
      UsdShadeShader texCoordReader = UsdDefineOrGet<UsdShadeShader>(actorCache.Stage, matCache.TexCoordReaderPath_mdl, newMat);
      assert(texCoordReader);
      UsdShadeOutput tcReaderOutput = InitializeUsdMdlGraphNode(texCoordReader, OmniConnectTokens->data_lookup_float2, SdfValueTypeNames->Float2);
      texCoordReader.CreateInput(OmniConnectTokens->name, SdfValueTypeNames->String).Set(TEX_PRIMVAR_NAME.GetString()); // Set fixed to st

      // Create a sampler
      UsdShadeShader sampler = UsdDefineOrGet<UsdShadeShader>(actorCache.Stage, matCache.SamplerPath_mdl, newMat);
      assert(sampler);
      sampler.CreateInput(OmniConnectTokens->coord, SdfValueTypeNames->Float2).ConnectToSource(tcReaderOutput);
      UsdShadeOutput samplerOutput = sampler.CreateOutput(OmniConnectTokens->out, SdfValueTypeNames->Float4);

      // xyz, w, construct color nodes to read texture data
      UsdShadeShader samplerXyz = UsdDefineOrGet<UsdShadeShader>(actorCache.Stage, matCache.SamplerXyzPath_mdl, newMat);
      assert(samplerXyz);
      UsdShadeOutput samplerXyzOut = InitializeUsdMdlGraphNode(samplerXyz, OmniConnectTokens->xyz, SdfValueTypeNames->Float3, constring::mdlAuxAssetName);
      samplerXyz.CreateInput(OmniConnectTokens->a, SdfValueTypeNames->Float4).ConnectToSource(samplerOutput);

      UsdShadeShader samplerColor = UsdDefineOrGet<UsdShadeShader>(actorCache.Stage, matCache.SamplerColorPath_mdl, newMat);
      assert(samplerColor);
      InitializeUsdMdlGraphNode(samplerColor, OmniConnectTokens->construct_color, SdfValueTypeNames->Color3f, constring::mdlAuxAssetName);
      samplerColor.CreateInput(OmniConnectTokens->a, SdfValueTypeNames->Float3).ConnectToSource(samplerXyzOut);

      UsdShadeShader samplerOpacity = UsdDefineOrGet<UsdShadeShader>(actorCache.Stage, matCache.SamplerOpacityPath_mdl, newMat);
      assert(samplerOpacity);
      InitializeUsdMdlGraphNode(samplerOpacity, OmniConnectTokens->w, SdfValueTypeNames->Float, constring::mdlAuxAssetName);
      samplerOpacity.CreateInput(OmniConnectTokens->a, SdfValueTypeNames->Float4).ConnectToSource(samplerOutput);

      UsdShadeShader opacityMul = UsdDefineOrGet<UsdShadeShader>(actorCache.Stage, matCache.OpacityMulPath_mdl, newMat);
      assert(opacityMul);
      UsdShadeOutput opacityMulOut = InitializeUsdMdlGraphNode(opacityMul, OmniConnectTokens->mul_float, SdfValueTypeNames->Float);
      opacityMul.CreateInput(OmniConnectTokens->a, SdfValueTypeNames->Float).Set(1.0f);
      opacityMul.CreateInput(OmniConnectTokens->b, SdfValueTypeNames->Float).Set(1.0f);
      usdShader.GetInput(OmniConnectTokens->opacity_constant).ConnectToSource(opacityMulOut);
    }

    output = usdShader.CreateOutput(OmniConnectTokens->out, SdfValueTypeNames->Token);

#if USE_CUSTOM_POINT_SHADER
    UsdShadeShader pointShader = UsdDefineOrGet<UsdShadeShader>(actorCache.Stage, matCache.MdlPointShadPath, newMat);
    CreateUsdMdlSurfaceShader(actorCache, matCache, pointShader, mdlNames, newMat, true);
#endif
  }
#endif

  return output; 
}

UsdShadeOutput CreateUsdMdlVolumeShader(UsdShadeShader& usdShader)
{
  // Create output
  return usdShader.CreateOutput(OmniConnectTokens->out, SdfValueTypeNames->Token);
}

UsdShadeOutput CreateUsdMdlMeshVolumeShader(UsdShadeShader& usdShader, OmniConnectMdlNames& mdlNames)
{
  usdShader.CreateImplementationSourceAttr(VtValue(OmniConnectTokens->sourceAsset));
  usdShader.SetSourceAssetSubIdentifier(OmniConnectTokens->OmniVolumeDensity, OmniConnectTokens->mdl);
  usdShader.SetSourceAsset(mdlNames.MeshVolumeMdl, OmniConnectTokens->mdl);

  return usdShader.CreateOutput(OmniConnectTokens->out, SdfValueTypeNames->Token);
}

void ResetUsdMdlShader(UsdShadeShader & usdShader,
  OmniConnectActorCache& actorCache, OmniConnectMatCache& matCache
)
{
  usdShader.GetInput(OmniConnectTokens->diffuse_color_constant).GetAttr().Clear();
  usdShader.GetInput(OmniConnectTokens->emissive_color).GetAttr().Clear();
  usdShader.GetInput(OmniConnectTokens->emissive_intensity).GetAttr().Clear();
  usdShader.GetInput(OmniConnectTokens->enable_emission).GetAttr().Clear();
  usdShader.GetInput(OmniConnectTokens->opacity_constant).GetAttr().Clear();
  usdShader.GetInput(OmniConnectTokens->reflection_roughness_constant).GetAttr().Clear();
  usdShader.GetInput(OmniConnectTokens->metallic_constant).GetAttr().Clear();

#if USE_CUSTOM_MDL
  usdShader.GetInput(OmniConnectTokens->ior_constant).GetAttr().Clear();
#else
  usdShader.GetInput(OmniConnectTokens->enable_opacity).GetAttr().Clear();

  UsdShadeShader opacityMul = UsdShadeShader::Get(actorCache.Stage, matCache.OpacityMulPath_mdl);
  opacityMul.GetInput(OmniConnectTokens->a).GetAttr().Clear();
  opacityMul.GetInput(OmniConnectTokens->b).GetAttr().Clear();

#if USE_CUSTOM_POINT_SHADER
  UsdShadeShader pointShader = UsdShadeShader::Get(actorCache.Stage, matCache.MdlPointShadPath);
  pointShader.GetInput(OmniConnectTokens->diffuse_color_constant).GetAttr().Clear();
  pointShader.GetInput(OmniConnectTokens->emissive_color).GetAttr().Clear();
  pointShader.GetInput(OmniConnectTokens->emissive_intensity).GetAttr().Clear();
  pointShader.GetInput(OmniConnectTokens->enable_emission).GetAttr().Clear();
  pointShader.GetInput(OmniConnectTokens->opacity_constant).GetAttr().Clear();
  pointShader.GetInput(OmniConnectTokens->reflection_roughness_constant).GetAttr().Clear();
  pointShader.GetInput(OmniConnectTokens->metallic_constant).GetAttr().Clear();
  pointShader.GetInput(OmniConnectTokens->ior_constant).GetAttr().Clear();
#endif
#endif
}

void UpdateUsdMdlShader(OmniConnectActorCache& actorCache, OmniConnectMatCache& matCache, const OmniConnectTexCache* texCache, 
  const OmniConnectMaterialData& omniMatData, UsdShadeShader& usdShader, const OmniConnectMdlNames& mdlNames, double animTimeStep
#if USE_CUSTOM_POINT_SHADER
  , bool updateCustomShader
#endif
  )
{
  TimeEvaluator<OmniConnectMaterialData> timeEval(omniMatData, animTimeStep);
  typedef OmniConnectMaterialData::DataMemberId DMI;

  bool useTexture = omniMatData.TexId != -1;
  GfVec3f difColor(omniMatData.Diffuse); ocutils::SrgbToLinear3(&difColor[0]);
  GfVec3f emColor(omniMatData.Emissive); ocutils::SrgbToLinear3(&emColor[0]);

  usdShader.GetInput(OmniConnectTokens->emissive_color).Set(emColor, timeEval.Eval(DMI::EMISSIVE));
  usdShader.GetInput(OmniConnectTokens->emissive_intensity).Set(omniMatData.EmissiveIntensity, timeEval.Eval(DMI::EMISSIVEINTENSITY));
  usdShader.GetInput(OmniConnectTokens->enable_emission).Set((omniMatData.EmissiveTextureData != nullptr) || (omniMatData.EmissiveIntensity > 0.0), timeEval.Eval(DMI::EMISSIVEINTENSITY));
  usdShader.GetInput(OmniConnectTokens->reflection_roughness_constant).Set(omniMatData.Roughness, timeEval.Eval(DMI::ROUGHNESS));
  usdShader.GetInput(OmniConnectTokens->metallic_constant).Set(omniMatData.Metallic, timeEval.Eval(DMI::METALLIC));

#if USE_CUSTOM_POINT_SHADER
  if(updateCustomShader)
#endif
#if USE_CUSTOM_MDL || USE_CUSTOM_POINT_SHADER
  {
    if (useTexture)
    {
      assert(texCache);
      UsdShadeInput diffTexInput = usdShader.GetInput(OmniConnectTokens->diffuse_texture);

      //Handle creation/clearing of this attribute seperately
      if (!diffTexInput)
        diffTexInput = usdShader.CreateInput(OmniConnectTokens->diffuse_texture, SdfValueTypeNames->Asset);
      else if (!texCache->TimeVarying)
        diffTexInput.GetAttr().Clear();

      diffTexInput.Set(
        texCache->TimeVarying ? texCache->TimedSceneRelTexturePath : texCache->SceneRelTexturePath,
        texCache->TimeVarying ? timeEval.TimeCode : timeEval.DefaultTime);

      usdShader.GetInput(OmniConnectTokens->diffuse_wrapmode_u).Set((int)texCache->WrapS);
      usdShader.GetInput(OmniConnectTokens->diffuse_wrapmode_v).Set((int)texCache->WrapT);
    }
    else
    {
      usdShader.GetPrim().RemoveProperty(TfToken("inputs:diffuse_texture"));
    }

    usdShader.GetInput(OmniConnectTokens->vertexcolor_coordinate_index).Set((!useTexture && omniMatData.UseVertexColors) ? 1 : -1);

    usdShader.GetInput(OmniConnectTokens->diffuse_color_constant).Set(difColor, timeEval.Eval(DMI::DIFFUSE));
    usdShader.GetInput(OmniConnectTokens->opacity_constant).Set(omniMatData.Opacity, timeEval.Eval(DMI::OPACITY));
    usdShader.GetInput(OmniConnectTokens->ior_constant).Set(omniMatData.Ior, timeEval.Eval(DMI::IOR));

    if (!omniMatData.HasTranslucency)
      usdShader.SetSourceAsset(mdlNames.OpaqueMdl, OmniConnectTokens->mdl);
    else
      usdShader.SetSourceAsset(mdlNames.TranslucentMdl, OmniConnectTokens->mdl);
  }
#endif
#if USE_CUSTOM_POINT_SHADER
  else 
#endif
#if !USE_CUSTOM_MDL
  {
    UsdShadeShader opacityMul = UsdShadeShader::Get(actorCache.Stage, matCache.OpacityMulPath_mdl);
    assert(opacityMul);

    if (useTexture)
    {
      // Set the sampler to the diffuse color
      UsdShadeShader sampler = UsdShadeShader::Get(actorCache.Stage, matCache.SamplerPath_mdl);
      assert(sampler);

      // Material->texture binding could change, which would mean a different reference
      sampler.GetPrim().GetReferences().ClearReferences();
      sampler.GetPrim().GetReferences().AddInternalReference(texCache->TexturePrimPath_mdl); 

      UsdShadeShader samplerColor = UsdShadeShader::Get(actorCache.Stage, matCache.SamplerColorPath_mdl);
      assert(samplerColor);
      UsdShadeOutput samplerColorOut = samplerColor.GetOutput(OmniConnectTokens->out);
      UsdShadeShader samplerOpacity = UsdShadeShader::Get(actorCache.Stage, matCache.SamplerOpacityPath_mdl);
      assert(samplerOpacity);
      UsdShadeOutput samplerOpacityOut = samplerOpacity.GetOutput(OmniConnectTokens->out);

      //Bind the texture to the diffuse color of the shader
      usdShader.GetInput(OmniConnectTokens->diffuse_color_constant).ConnectToSource(samplerColorOut);
      if(omniMatData.OpacityMapped)
        opacityMul.GetInput(OmniConnectTokens->a).ConnectToSource(samplerOpacityOut);
    }
    else
    {
      if (omniMatData.UseVertexColors)
      {
        UsdShadeShader vertexColorReader = UsdShadeShader::Get(actorCache.Stage, matCache.VertexColorReaderPath_mdl);
        assert(vertexColorReader);
        UsdShadeOutput vcReaderOutput = vertexColorReader.GetOutput(OmniConnectTokens->out);

        UsdShadeShader vertexOpacityPow = UsdShadeShader::Get(actorCache.Stage, matCache.VertexOpacityPowPath_mdl);
        assert(vertexOpacityPow);
        UsdShadeOutput voPowOutput = vertexOpacityPow.GetOutput(OmniConnectTokens->out);

        usdShader.GetInput(OmniConnectTokens->diffuse_color_constant).ConnectToSource(vcReaderOutput);
        if(omniMatData.OpacityMapped)
          opacityMul.GetInput(OmniConnectTokens->a).ConnectToSource(voPowOutput);
      }
      else
      {
        UsdShadeInput diffInput = usdShader.GetInput(OmniConnectTokens->diffuse_color_constant);
        diffInput.DisconnectSource();
        diffInput.Set(difColor, timeEval.Eval(DMI::DIFFUSE));
      }
    }

    if(!omniMatData.OpacityMapped || (!useTexture && !omniMatData.UseVertexColors))
    {
      UsdShadeInput opacityInput = opacityMul.GetInput(OmniConnectTokens->a);
      opacityInput.DisconnectSource();
      opacityInput.Set(1.0f, timeEval.Eval(DMI::OPACITY));
    }
    opacityMul.GetInput(OmniConnectTokens->b).Set(omniMatData.Opacity, timeEval.Eval(DMI::OPACITY));
    usdShader.GetInput(OmniConnectTokens->enable_opacity).Set(omniMatData.HasTranslucency, timeEval.Eval(DMI::OPACITY));

#if USE_CUSTOM_POINT_SHADER
    UsdShadeMaterial material = UsdShadeMaterial::Get(actorCache.Stage, matCache.SdfMatName);
    UsdShadeShader pointShader = UsdShadeShader::Get(actorCache.Stage, matCache.MdlPointShadPath);
    UpdateUsdMdlShader(actorCache, matCache, texCache, omniMatData, pointShader, mdlNames, animTimeStep, true);

    if(omniMatData.UsePointShader)
      material.GetSurfaceOutput(OmniConnectTokens->mdl).ConnectToSource(pointShader.GetOutput(OmniConnectTokens->surface));
    else
      material.GetSurfaceOutput(OmniConnectTokens->mdl).ConnectToSource(usdShader.GetOutput(OmniConnectTokens->out));
#endif
  }
#endif
}

#if !USE_CUSTOM_MDL
UsdShadeShader InitializeMdlTexture(const OmniConnectActorCache& actorCache, const OmniConnectSamplerData& samplerData, const OmniConnectTexCache& texCache)
{
  UsdShadeShader sampler = UsdShadeShader::Define(actorCache.Stage, texCache.TexturePrimPath_mdl);
  assert(sampler);

  // Static properties
  InitializeUsdMdlGraphNode(sampler, OmniConnectTokens->lookup_float4, SdfValueTypeNames->Float4);
  sampler.CreateInput(OmniConnectTokens->wrap_u, SdfValueTypeNames->Int).Set(TextureWrapInt(samplerData.WrapS));
  sampler.CreateInput(OmniConnectTokens->wrap_v, SdfValueTypeNames->Int).Set(TextureWrapInt(samplerData.WrapT));

  // Timevarying properties
  UsdShadeInput texInput = sampler.CreateInput(OmniConnectTokens->tex, SdfValueTypeNames->Asset);
  texInput.GetAttr().SetMetadata(OmniConnectTokens->colorSpace, VtValue("sRGB"));
  texInput.Set(texCache.SceneRelTexturePath);

  return sampler;
}

void UpdateMdlTexture(const OmniConnectTexCache& texCache, const UsdShadeShader& sampler, double animTimeStep)
{
  UsdShadeInput fileInput = sampler.GetInput(OmniConnectTokens->tex);

  // Clear only when not timevarying, as timesamples override the default value anyway
  if(!texCache.TimeVarying)
    fileInput.GetAttr().Clear();

  // Set the texture file
  fileInput.Set(
      texCache.TimeVarying ? texCache.TimedSceneRelTexturePath : texCache.SceneRelTexturePath,
      texCache.TimeVarying ? UsdTimeCode(animTimeStep) : UsdTimeCode::Default());
}
#endif

#ifdef USE_INDEX_MATERIALS
UsdShadeOutput CreateUsdIndexVolumeShader(OmniConnectActorCache& actorCache, OmniConnectMatCache& matCache, UsdShadeShader& indexShader, bool newMat)
{
  UsdPrim colorMapPrim = newMat ?
    actorCache.Stage->DefinePrim(matCache.ColorMapShadPath, OmniConnectTokens->Colormap) :
    actorCache.Stage->GetPrimAtPath(matCache.ColorMapShadPath);
  assert(colorMapPrim);

  UsdShadeConnectableAPI colorMapConn(colorMapPrim);
  UsdShadeOutput colorMapShadOut = colorMapConn.CreateOutput(OmniConnectTokens->colormap, SdfValueTypeNames->Token);

  colorMapPrim.CreateAttribute(OmniConnectTokens->domainBoundaryMode , SdfValueTypeNames->Token).Set(OmniConnectTokens->clampToEdge);
  colorMapPrim.CreateAttribute(OmniConnectTokens->colormapSource , SdfValueTypeNames->Token).Set(OmniConnectTokens->colormapValues); 
  colorMapPrim.CreateAttribute(OmniConnectTokens->domain, SdfValueTypeNames->Float2);
  colorMapPrim.CreateAttribute(OmniConnectTokens->colormapValues, SdfValueTypeNames->Float4Array);

  indexShader.CreateInput(OmniConnectTokens->colormap, SdfValueTypeNames->Token).ConnectToSource(colorMapShadOut);
  return indexShader.CreateOutput(OmniConnectTokens->volume, SdfValueTypeNames->Token);
}

void ResetUsdIndexVolumeShader(OmniConnectActorCache& actorCache, OmniConnectMatCache& matCache)
{
  UsdPrim colorMapPrim = actorCache.Stage->GetPrimAtPath(matCache.ColorMapShadPath);
  colorMapPrim.GetAttribute(OmniConnectTokens->domain).Clear();
  colorMapPrim.GetAttribute(OmniConnectTokens->colormapValues).Clear();
}

void UpdateUsdIndexVolumeShader(OmniConnectActorCache& actorCache, OmniConnectMatCache& matCache, const OmniConnectTexCache* texCache, 
  const OmniConnectMaterialData& omniMatData, UsdShadeShader& indexShader, double animTimeStep)
{
  TimeEvaluator<OmniConnectMaterialData> timeEval(omniMatData, animTimeStep);
  typedef OmniConnectMaterialData::DataMemberId DMI;

  // Renormalize the value range based on the volume data type (see CopyToGrid in the volumewriter)
  double minMax[2] = {0, 1};
  switch (omniMatData.VolumeDataType)
  {
  case OmniConnectType::CHAR:
    minMax[0] = std::numeric_limits<char>::min();
    minMax[1] = std::numeric_limits<char>::max();
    break;
  case OmniConnectType::UCHAR:
    minMax[0] = std::numeric_limits<unsigned char>::min();
    minMax[1] = std::numeric_limits<unsigned char>::max();
    break;
  case OmniConnectType::SHORT:
    minMax[0] = std::numeric_limits<short>::min();
    minMax[1] = std::numeric_limits<short>::max();
    break;
  case OmniConnectType::USHORT:
    minMax[0] = std::numeric_limits<unsigned short>::min();
    minMax[1] = std::numeric_limits<unsigned short>::max();
    break;
  }
  double numericRangeFactor = 1.0 / (minMax[1] - minMax[0]);
  GfVec2f valueRange(GfVec2d(
    (omniMatData.TfData.TfValueRange[0] - minMax[0]) * numericRangeFactor,
    (omniMatData.TfData.TfValueRange[1] - minMax[0]) * numericRangeFactor));

  // Set the transfer function value array from opacities and colors
  VtVec4fArray tfValues(omniMatData.TfData.TfNumValues);
  assert(omniMatData.TfData.TfColorsType == OmniConnectType::FLOAT3);
  assert(omniMatData.TfData.TfOpacitiesType == OmniConnectType::FLOAT);
  const float* tfColors = static_cast<const float*>(omniMatData.TfData.TfColors);
  const float* tfOpacities = static_cast<const float*>(omniMatData.TfData.TfOpacities);

  for(int i = 0; i < tfValues.size(); ++i)
  {
    tfValues[i] = GfVec4f(
      tfColors[i*3],
      tfColors[i*3+1],
      tfColors[i*3+2],
      tfOpacities[i]
      ); 
  }

  // Set domain and colormap values
  UsdPrim colorMapPrim = actorCache.Stage->GetPrimAtPath(matCache.ColorMapShadPath);
  colorMapPrim.GetAttribute(OmniConnectTokens->domain).Set(valueRange, timeEval.Eval(DMI::TF));
  colorMapPrim.GetAttribute(OmniConnectTokens->colormapValues).Set(tfValues, timeEval.Eval(DMI::TF));
}
#endif

#endif
