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

#ifndef OmniConnectVolumeWriter_h
#define OmniConnectVolumeWriter_h

#include "OmniConnectData.h"

#ifdef _WIN32
#  define OmniConnect_Vol_DECL __cdecl
#  ifdef OmniConnect_Volume_EXPORTS
#    define OmniConnect_Vol_INTERFACE __declspec(dllexport)
#  else
#    define OmniConnect_Vol_INTERFACE __declspec(dllimport)
#  endif
#else
#    define OmniConnect_Vol_DECL
#    define OmniConnect_Vol_INTERFACE
#endif

class OmniConnectVolumeWriterI
{
  public:

    virtual bool Initialize(OmniConnectLogCallback logCallback, void* logUserData) = 0;

    virtual void ToVDB(const OmniConnectVolumeData& volumeData,
      OmniConnectGenericArray* updatedGenericArrays, size_t numUga) = 0;

    virtual void GetSerializedVolumeData(const char*& data, size_t& size) = 0;

    virtual void SetConvertDoubleToFloat(bool convert) = 0;

    virtual void Release() = 0; // Accommodate change of CRT
};

extern "C" OmniConnect_Vol_INTERFACE OmniConnectVolumeWriterI* OmniConnect_Vol_DECL Create_VolumeWriter();



#endif