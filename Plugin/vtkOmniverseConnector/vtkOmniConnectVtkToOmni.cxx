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

#include "vtkOmniConnectVtkToOmni.h"

#include "vtkDataArray.h"
#include "vtkOmniConnectPass.h"

void GetOmniConnectSettings(const vtkOmniConnectSettings& vtkSettings, OmniConnectSettings& omniConnectSettings)
{
  omniConnectSettings.OmniServer = vtkSettings.OmniServer.c_str();
  omniConnectSettings.OmniWorkingDirectory = vtkSettings.OmniWorkingDirectory.c_str();
  omniConnectSettings.LocalOutputDirectory = vtkSettings.LocalOutputDirectory.c_str();
  omniConnectSettings.RootLevelFileName = vtkSettings.RootLevelFileName.c_str();
  omniConnectSettings.OutputLocal = vtkSettings.OutputLocal;
  omniConnectSettings.OutputBinary = vtkSettings.OutputBinary;
  omniConnectSettings.UpAxis = vtkSettings.UpAxis;
  omniConnectSettings.UsePointInstancer = vtkSettings.UsePointInstancer;
  omniConnectSettings.UseMeshVolume = vtkSettings.UseMeshVolume;
  omniConnectSettings.CreateNewOmniSession = vtkSettings.CreateNewOmniSession;
}

OmniConnectType GetOmniConnectType(vtkDataArray* dataArray)
{
  static_assert(sizeof(vtkIdType) == sizeof(uint64_t), "vtkIdType is not 64-bit");

  static const OmniConnectType formatConversionTable[][4] =
  { { OmniConnectType::CHAR,       OmniConnectType::CHAR2,      OmniConnectType::CHAR3,      OmniConnectType::CHAR4 },
    { OmniConnectType::UCHAR,      OmniConnectType::UCHAR2,     OmniConnectType::UCHAR3,     OmniConnectType::UCHAR4},
    { OmniConnectType::SHORT,      OmniConnectType::SHORT2,     OmniConnectType::SHORT3,     OmniConnectType::SHORT4},
    { OmniConnectType::USHORT,     OmniConnectType::USHORT2,    OmniConnectType::USHORT3,    OmniConnectType::USHORT4},
    { OmniConnectType::INT,        OmniConnectType::INT2,       OmniConnectType::INT3,       OmniConnectType::INT4},
    { OmniConnectType::UINT,       OmniConnectType::UINT2,      OmniConnectType::UINT3,      OmniConnectType::UINT4},
    { OmniConnectType::LONG,       OmniConnectType::LONG2,      OmniConnectType::LONG3,      OmniConnectType::LONG4},
    { OmniConnectType::ULONG,      OmniConnectType::ULONG2,     OmniConnectType::ULONG3,     OmniConnectType::ULONG4},
    { OmniConnectType::FLOAT,      OmniConnectType::FLOAT2,     OmniConnectType::FLOAT3,     OmniConnectType::FLOAT4},
    { OmniConnectType::DOUBLE,     OmniConnectType::DOUBLE2,    OmniConnectType::DOUBLE3,    OmniConnectType::DOUBLE4},
    { OmniConnectType::UNDEFINED,  OmniConnectType::UNDEFINED,  OmniConnectType::UNDEFINED,  OmniConnectType::UNDEFINED}
  };

  int tableIndex = 0;
  switch (dataArray->GetDataType())
  {
  case VTK_CHAR: tableIndex = 0; break;
  case VTK_SIGNED_CHAR: tableIndex = 0; break;
  case VTK_UNSIGNED_CHAR: tableIndex = 1; break;
  case VTK_SHORT: tableIndex = 2; break;
  case VTK_UNSIGNED_SHORT: tableIndex = 3; break;
  case VTK_INT: tableIndex = 4; break;
  case VTK_UNSIGNED_INT: tableIndex = 5; break;
  case VTK_LONG: tableIndex = 6; break;
  case VTK_UNSIGNED_LONG: tableIndex = 7; break;
  case VTK_FLOAT: tableIndex = 8; break;
  case VTK_DOUBLE: tableIndex = 9; break;
  case VTK_ID_TYPE: tableIndex = 6; break;
  default: tableIndex = 10; break;
  }

  int numComps = dataArray->GetNumberOfComponents();

  OmniConnectType resultType;
  if (numComps > 4)
    resultType = OmniConnectType::UNDEFINED;
  else
    resultType = formatConversionTable[tableIndex][numComps - 1];

  return resultType;
}

size_t GetOmniConnectTypeSize(OmniConnectType type)
{
  size_t result = 0;
  switch (type)
  {
    case OmniConnectType::CHAR: result = sizeof(char); break;
    case OmniConnectType::CHAR2: result = sizeof(char)*2; break;
    case OmniConnectType::CHAR3: result = sizeof(char)*3; break;
    case OmniConnectType::CHAR4: result = sizeof(char)*4; break;
    case OmniConnectType::UCHAR: result = sizeof(char); break;
    case OmniConnectType::UCHAR2: result = sizeof(char)*2; break;
    case OmniConnectType::UCHAR3: result = sizeof(char)*3; break;
    case OmniConnectType::UCHAR4: result = sizeof(char)*4; break;
    case OmniConnectType::SHORT: result = sizeof(short); break;
    case OmniConnectType::SHORT2: result = sizeof(short)*2; break;
    case OmniConnectType::SHORT3: result = sizeof(short)*3; break;
    case OmniConnectType::SHORT4: result = sizeof(short)*4; break;
    case OmniConnectType::USHORT: result = sizeof(short); break;
    case OmniConnectType::USHORT2: result = sizeof(short)*2; break;
    case OmniConnectType::USHORT3: result = sizeof(short)*3; break;
    case OmniConnectType::USHORT4: result = sizeof(short)*4; break;
    case OmniConnectType::INT: result = sizeof(int); break;
    case OmniConnectType::INT2: result = sizeof(int)*2; break;
    case OmniConnectType::INT3: result = sizeof(int)*3; break;
    case OmniConnectType::INT4: result = sizeof(int)*4; break;
    case OmniConnectType::UINT: result = sizeof(int); break;
    case OmniConnectType::UINT2: result = sizeof(int)*2; break;
    case OmniConnectType::UINT3: result = sizeof(int)*3; break;
    case OmniConnectType::UINT4: result = sizeof(int)*4; break;
    case OmniConnectType::LONG: result = sizeof(int64_t); break;
    case OmniConnectType::LONG2: result = sizeof(int64_t)*2; break;
    case OmniConnectType::LONG3: result = sizeof(int64_t)*3; break;
    case OmniConnectType::LONG4: result = sizeof(int64_t)*4; break;
    case OmniConnectType::ULONG: result = sizeof(int64_t); break;
    case OmniConnectType::ULONG2: result = sizeof(int64_t)*2; break;
    case OmniConnectType::ULONG3: result = sizeof(int64_t)*3; break;
    case OmniConnectType::ULONG4: result = sizeof(int64_t)*4; break;
    case OmniConnectType::FLOAT: result = sizeof(float); break;
    case OmniConnectType::FLOAT2: result = sizeof(float)*2; break;
    case OmniConnectType::FLOAT3: result = sizeof(float)*3; break;
    case OmniConnectType::FLOAT4: result = sizeof(float)*4; break;
    case OmniConnectType::DOUBLE: result = sizeof(double); break;
    case OmniConnectType::DOUBLE2: result = sizeof(double)*2; break;
    case OmniConnectType::DOUBLE3: result = sizeof(double)*3; break;
    case OmniConnectType::DOUBLE4: result = sizeof(double)*4; break;
    default: result = 0; break;
  }

  return result;
}