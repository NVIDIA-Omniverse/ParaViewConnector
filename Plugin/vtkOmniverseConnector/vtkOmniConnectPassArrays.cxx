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

#include "vtkOmniConnectPassArrays.h"

#include "vtkObjectFactory.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkDemandDrivenPipeline.h"
#include "vtkUnsignedIntArray.h"
#include "vtkDoubleArray.h"
#include "vtkFieldData.h"

vtkStandardNewMacro(vtkOmniConnectPassArrays);

const char* vtkOmniConnectPassArrays::PassArraysFlagName = "OmniConnectPassArraysFlag";

//----------------------------------------------------------------------------
vtkOmniConnectPassArrays::vtkOmniConnectPassArrays()
{
}

//----------------------------------------------------------------------------
vtkOmniConnectPassArrays::~vtkOmniConnectPassArrays()
{
}

//----------------------------------------------------------------------------
int vtkOmniConnectPassArrays::RequestData(
  vtkInformation* request,
  vtkInformationVector** inputVector,
  vtkInformationVector* outputVector)
{
  int res = this->Superclass::RequestData(request, inputVector, outputVector);

  //if (request->Has(vtkDemandDrivenPipeline::REQUEST_INFORMATION()))

  vtkInformation* outInfo = outputVector->GetInformationObject(0);
  vtkDataObject* outputData = outInfo->Get(vtkDataObject::DATA_OBJECT());
  //vtkInformation* outputDataInfo = outputData->GetInformation();
  vtkFieldData* fieldData = outputData->GetFieldData();

  vtkUnsignedIntArray* omnipassFlag = vtkUnsignedIntArray::New();
  omnipassFlag->SetNumberOfValues(1);
  omnipassFlag->SetName(PassArraysFlagName);
  omnipassFlag->SetValue(0, ConvertDoubleToFloat);
  fieldData->AddArray(omnipassFlag);
  omnipassFlag->Delete();

  return res;
}

//----------------------------------------------------------------------------
void vtkOmniConnectPassArrays::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
