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

#include "vtkPVOmniConnectProxyUrlInfo.h"

#include "vtkPVOmniConnectProxy.h"
#include "vtkOmniConnectLogCallback.h"
#include "vtkClientServerStream.h"
#include "OmniConnect.h"

vtkStandardNewMacro(vtkPVOmniConnectProxyUrlInfo);

//----------------------------------------------------------------------------
vtkPVOmniConnectProxyUrlInfo::vtkPVOmniConnectProxyUrlInfo()
{
  this->RootOnly = 1;
}

//----------------------------------------------------------------------------
vtkPVOmniConnectProxyUrlInfo::~vtkPVOmniConnectProxyUrlInfo()
{}

//----------------------------------------------------------------------------
void vtkPVOmniConnectProxyUrlInfo::CopyFromObject(vtkObject* object)
{
  vtkPVOmniConnectProxy* proxy = vtkPVOmniConnectProxy::SafeDownCast(object);
  if (!proxy)
  {
    vtkOmniConnectLogCallback::Callback(OmniConnectLogLevel::ERR, nullptr, "Can collect information only from a vtkPVOmniConnectProxy.");
    return;
  }

  UrlInfoList = proxy->GetUrlInfoList_Server(proxy->GetUrlInfoList_Url());
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectProxyUrlInfo::CopyToStream(vtkClientServerStream* stream)
{
  *stream << vtkClientServerStream::Reply;
  *stream << UrlInfoList.size;
  
  for(size_t i = 0; i < UrlInfoList.size; ++i)
  {
    const OmniConnectUrlInfo& urlInfo = UrlInfoList.infos[i];
    *stream
      << urlInfo.Url
      << urlInfo.Etag
      << urlInfo.Author
      << urlInfo.ModifiedTime
      << urlInfo.Size
      << (int)urlInfo.IsFile;
  }

  *stream << vtkClientServerStream::End;
}

#define CLIENTSERVERSTREAM_GET_ARGUMENT(argName) \
  if (!stream->GetArgument(0, idx++, &argName)) \
  { \
    vtkOmniConnectLogCallback::Callback(OmniConnectLogLevel::ERR, nullptr, "Error parsing 'argName' from message in vtkPVOmniConnectProxyUrlInfo."); \
    return; \
  } \

//----------------------------------------------------------------------------
void vtkPVOmniConnectProxyUrlInfo::CopyFromStream(const vtkClientServerStream* stream)
{  
  int idx = 0;

  size_t infoListSize = 0; 
  CLIENTSERVERSTREAM_GET_ARGUMENT(infoListSize);

  UrlInfos.resize(infoListSize);
  UrlStrings.resize(infoListSize);
  for(size_t i = 0; i < infoListSize; ++i)
  {
    OmniConnectUrlInfo& urlInfo = UrlInfos[i];
    UrlStringsType& urlStrings = UrlStrings[i];

    CLIENTSERVERSTREAM_GET_ARGUMENT(urlStrings.Url);
    CLIENTSERVERSTREAM_GET_ARGUMENT(urlStrings.Etag);
    CLIENTSERVERSTREAM_GET_ARGUMENT(urlStrings.Author);
    CLIENTSERVERSTREAM_GET_ARGUMENT(urlInfo.ModifiedTime);
    CLIENTSERVERSTREAM_GET_ARGUMENT(urlInfo.Size);

    int isFile;
    CLIENTSERVERSTREAM_GET_ARGUMENT(isFile);
    urlInfo.IsFile = isFile;
  }  

  // Copy over string pointers *after* lists have been constructed 
  for(size_t i = 0; i < infoListSize; ++i)
  {
    UrlStringsType& urlStrings = UrlStrings[i];
    OmniConnectUrlInfo& urlInfo = UrlInfos[i];

    urlInfo.Url = urlStrings.Url.c_str();
    urlInfo.Author = urlStrings.Author.c_str();
    urlInfo.Etag = urlStrings.Etag.c_str();
  }

  UrlInfoList = { UrlInfos.data(), UrlInfos.size() };
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectProxyUrlInfo::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}