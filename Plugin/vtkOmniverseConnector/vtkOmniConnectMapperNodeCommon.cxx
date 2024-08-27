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

#include "vtkOmniConnectMapperNodeCommon.h"
#include "vtkOmniConnectPassArrays.h"
#include "vtkUnsignedIntArray.h"
#include "vtkDataSet.h"
#include "vtkFieldData.h"
#include "vtkMath.h"
#include "OmniConnect.h"
#include "vtkQuaternion.h"

void SetUpdatesToPerform(OmniConnectMaterialData& omniMatData, bool forceAllUpdates)
{
  omniMatData.TimeVarying = forceAllUpdates ? OmniConnectMaterialData::DataMemberId::ALL : OmniConnectMaterialData::DataMemberId::NONE;
}

void SetConvertGenericArraysDoubleToFloat(OmniConnect* connector, vtkDataSet* dataSet, bool defaultValue)
{
  bool convertGenericToFloat = defaultValue;
  vtkDataArray* passArrays = dataSet->GetFieldData()->GetArray(vtkOmniConnectPassArrays::PassArraysFlagName);
  if(passArrays)
    convertGenericToFloat = vtkUnsignedIntArray::SafeDownCast(passArrays)->GetValue(0);
  connector->SetConvertGenericArraysDoubleToFloat(convertGenericToFloat);
}

void DirectionToQuaternionX(float* dir, float dirLength, float* quat)
{
  // Use X axis of glyph to orient along.

  if(dirLength == 0)
  { // identity rotation
    quat[0] = 1;
    quat[1] = 0;
    quat[2] = 0;
    quat[3] = 0;
    return;
  }

  float invDirLength = 1.0f / dirLength;
  float halfVec[3] = {
    dir[0] * invDirLength + 1.0f,
    dir[1] * invDirLength,
    dir[2] * invDirLength
  };
  float halfNorm = vtkMath::Normalize(halfVec); 
  float sinAxis[3] = { 0.0, -halfVec[2], halfVec[1] };
  float cosAngle = halfVec[0];
  if (halfNorm == 0.0f)
  {
    sinAxis[2] = 1.0f; //sinAxis*sin(pi/2) = (0,0,1)*sin(pi/2) = (0,0,1)
    // cosAngle = cos(pi/2) = 0;
  }

  quat[0] = cosAngle;
  quat[1] = sinAxis[0];
  quat[2] = sinAxis[1];
  quat[3] = sinAxis[2];
}

void DirectionToQuaternionY(float* dir, float dirLength, float* quat)
{
  // Use Y axis of glyph to orient along.

  if(dirLength == 0)
  { // identity rotation
    quat[0] = 1;
    quat[1] = 0;
    quat[2] = 0;
    quat[3] = 0;
    return;
  }

  // (dot(|segDir|, yAxis), cross(|segDir|, yAxis)) gives (cos(th), axis*sin(th)),
  // but rotation is represented by cos(th/2), axis*sin(th/2), ie. half the amount of rotation.
  // So calculate (dot(|halfVec|, yAxis), cross(|halfVec|, yAxis)) instead, with halfvec = |segDir|+yAxis.
  float invDirLength = 1.0f / dirLength;
  float halfVec[3] = {
    dir[0] * invDirLength,
    dir[1] * invDirLength + 1.0f,
    dir[2] * invDirLength
  };
  float halfNorm = vtkMath::Normalize(halfVec); // Normalizes halfVec and returns length

  // Cross yAxis (0,1,0) with halfVec (new Y axis) to get rotation axis * sin(angle/2)
  float sinAxis[3] = { halfVec[2], 0.0f, -halfVec[0] };
  // Dot for cos(angle/2)
  float cosAngle = halfVec[1];
  if (halfNorm == 0.0f) // In this case there is a 180 degree rotation (sinAxis==(0,0,0))
  {
    sinAxis[2] = 1.0f; //sinAxis*sin(pi/2) = (0,0,1)*sin(pi/2) = (0,0,1)
    // cosAngle = cos(pi/2) = 0;
  }

  quat[0] = cosAngle;
  quat[1] = sinAxis[0];
  quat[2] = sinAxis[1];
  quat[3] = sinAxis[2];
}

void RotationToQuaternion(float* rot, float* quat)
{
  vtkQuaternionf resultQuat;

  float angle = vtkMath::RadiansFromDegrees(rot[2]);
  vtkQuaternionf qz(cos(0.5f * angle), 0.0f, 0.0f, sin(0.5f * angle));

  angle = vtkMath::RadiansFromDegrees(rot[0]);
  vtkQuaternionf qx(cos(0.5f * angle), sin(0.5f * angle), 0.0f, 0.0f);

  angle = vtkMath::RadiansFromDegrees(rot[1]);
  vtkQuaternionf qy(cos(0.5f * angle), 0.0f, sin(0.5f * angle), 0.0f);

  resultQuat = qz * qx * qy;

  resultQuat.Get(quat);
}