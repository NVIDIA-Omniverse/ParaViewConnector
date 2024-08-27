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

#include "vtkOmniConnectImageWriter.h"
#include "vtkImageData.h"
#include "vtkPointData.h"
#include "vtkDataArray.h"
#include "vtkUnsignedCharArray.h"
#include "vtkErrorCode.h"
#include <cassert>

vtkOmniConnectImageWriter::vtkOmniConnectImageWriter()
  : Writer(vtkPNGWriter::New())
{}

vtkOmniConnectImageWriter::~vtkOmniConnectImageWriter()
{
  Writer->Delete();
}

bool vtkOmniConnectImageWriter::WriteData(void* omniImage)
{
  vtkImageData* vtkImage = (vtkImageData*)omniImage;

  // Hack to make sure that the border is removed (changes PV image data itself)
  if (ModifyBorder)
  {
    // Assuming a rather strict layout for color maps. If asserts are hit, expand this part of the code.
    assert(vtkImage->GetDimensions()[1] == 2);

    int numComponents = vtkImage->GetPointData()->GetScalars()->GetNumberOfComponents();

    unsigned char * srcPtr = (unsigned char *)vtkImage->GetScalarPointer(0, 0, 0);
    unsigned char * dstPtr = (unsigned char *)vtkImage->GetScalarPointer(0, 1, 0);
    memcpy(dstPtr, srcPtr, (vtkImage->GetDimensions()[0]) * numComponents);
  }

  Writer->SetInputData((vtkImageData*)omniImage);
  Writer->SetWriteToMemory(true);
  Writer->Write();
  return Writer->GetErrorCode() == vtkErrorCode::NoError;
}

void vtkOmniConnectImageWriter::GetResult(char*& imageData, int& imageDataSize)
{
  imageData = (char*)(Writer->GetResult()->GetPointer(0));
  imageDataSize = Writer->GetResult()->GetDataSize() * Writer->GetResult()->GetDataTypeSize();
}