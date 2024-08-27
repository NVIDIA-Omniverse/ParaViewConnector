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

#ifndef OmniConnectUsdUtils_h
#define OmniConnectUsdUtils_h

#include "OmniConnectData.h"

#if USE_CUSTOM_MDL
#define TEX_PRIMVAR_NAME OmniConnectTokens->st
#else
#define TEX_PRIMVAR_NAME OmniConnectTokens->texcoord0
#endif

#if USE_CUSTOM_MDL || USE_CUSTOM_POINT_SHADER
#define MDL_TOKEN_SEQ_CUSTOM \
  (PBR_Base) \
  (ior_constant) \
  (vertexcolor_coordinate_index) \
  (diffuse_wrapmode_u) \
  (diffuse_wrapmode_v) \
  (diffuse_texture) \
  (st1) \
  (st2)
#else
#define MDL_TOKEN_SEQ_CUSTOM
#endif

#define MDL_TOKEN_SEQ \
  (mdl) \
  (OmniPBR) \
  (sourceAsset) \
  (data_lookup_float) \
  (data_lookup_float2) \
  (data_lookup_float3) \
  (data_lookup_color) \
  (lookup_color) \
  (lookup_float4) \
  ((xyz, "xyz(float4)")) \
  ((w, "w(float4)")) \
  ((construct_color, "construct_color(float3)")) \
  ((mul_float, "multiply(float,float)")) \
  ((pow_float, "pow(float,float)")) \
  (b)\
  (name) \
  (tex) \
  (wrap_u) \
  (wrap_v) \
  (coord) \
  (texcoord0) \
  (colorSpace) \
  (enable_opacity)  \
  (enable_emission) \
  (diffuse_color_constant) \
  (opacity_constant) \
  (emissive_color) \
  (emissive_intensity) \
  (reflection_roughness_constant) \
  (metallic_constant) \
  (OmniVolumeDensity) \
  MDL_TOKEN_SEQ_CUSTOM

TF_DEFINE_PRIVATE_TOKENS(
  OmniConnectTokens,
  (Root)
  (extent)
  (faceVertexCounts)
  (faceVertexIndices)
  (points)
  (positions)
  (normals)
  (st)
  (result)
  (ids)
  (widths)
  (displayColor)
  (displayOpacity)
  (protoIndices)
  (orientations)
  (scales)
  (velocities)
  (angularVelocities)
  (invisibleIds)
  (curveVertexCounts)
  (filePath)
  (out)
  ((materialBinding, "material:binding"))

  // For usd preview surface
  (UsdPreviewSurface)
  (useSpecularWorkflow)
  (roughness)
  (opacity)
  (metallic)
  (ior)
  (diffuseColor)
  (specularColor)
  (emissiveColor)
  (surface)
  (varname)
  ((PrimStId, "UsdPrimvarReader_float2"))
  ((PrimDisplayColorId, "UsdPrimvarReader_float3"))

  // Volumes
  (density)
  (diffuse)
  (volume_density_texture)
  (isVolume)

  // Index
  (nvindex)
  (volume)
  (colormap)
  (Colormap)
  (colormapValues)
  (colormapSource)
  (domain)
  (domainBoundaryMode)
  (clampToEdge)

  // Mdl
  MDL_TOKEN_SEQ

  // Texture/Sampler
  (UsdUVTexture)
  (fallback)
  (rgb)
  (a)
  (file)
  (WrapS)
  (WrapT)
  (black)
  (clamp)
  (repeat)
  (mirror)

  //OmniConnect
  (MatId)
);

namespace
{
  template<class T>
  class TimeEvaluator
  {
  public:
    TimeEvaluator(const T& data, double timeStep)
      : Data(data)
      , TimeCode(timeStep)
    {
    }

    const UsdTimeCode& Eval(typename T::DataMemberId member) const
    {
      if ((Data.TimeVarying & member) != T::DataMemberId::NONE)
        return TimeCode;
      else
        return DefaultTime;
    }

    bool IsTimeVarying(typename T::DataMemberId member) const
    {
      return ((Data.TimeVarying & member) != T::DataMemberId::NONE);
    }

    const T& Data;
    const UsdTimeCode TimeCode;
    static const UsdTimeCode DefaultTime;
  };

  template<class T>
  const UsdTimeCode TimeEvaluator<T>::DefaultTime = UsdTimeCode::Default();

  template<>
  class TimeEvaluator<bool>
  {
  public:
    TimeEvaluator(bool timeVarying, double timeStep)
      : TimeVarying(timeVarying)
      , TimeCode(timeStep)
    {

    }

    const UsdTimeCode& Eval() const
    {
      if (TimeVarying)
        return TimeCode;
      else
        return DefaultTime;
    }

    bool TimeVarying;
    const UsdTimeCode TimeCode;
    static const UsdTimeCode DefaultTime;
  };

  const UsdTimeCode TimeEvaluator<bool>::DefaultTime = UsdTimeCode::Default();
}

TfToken TextureWrapToken(OmniConnectSamplerData::WrapMode wrapMode);
int TextureWrapInt(OmniConnectSamplerData::WrapMode wrapMode);

template<typename T>
T UsdDefineOrGet(const UsdStageRefPtr& stage, const SdfPath& path, bool define)
{
  if(define)
    return T::Define(stage, path);
  else
    return T::Get(stage, path);
}

#endif