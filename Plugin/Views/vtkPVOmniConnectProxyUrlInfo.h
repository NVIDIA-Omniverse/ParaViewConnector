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

#ifndef vtkPVOmniConnectProxyUrlInfo_h
#define vtkPVOmniConnectProxyUrlInfo_h

#include "OmniConnectViewsModule.h"
#include "vtkNew.h" // for vtkNew
#include "vtkObjectFactory.h"
#include "OmniConnectData.h"
#include "vtkPVInformation.h"

#include <string>
#include <vector>

class vtkClientServerStream;

class OMNICONNECTVIEWS_EXPORT vtkPVOmniConnectProxyUrlInfo : public vtkPVInformation
{
public:
  static vtkPVOmniConnectProxyUrlInfo* New();
  vtkTypeMacro(vtkPVOmniConnectProxyUrlInfo, vtkPVInformation);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * Transfer information about a single object into this object.
   * The object must be a vtkPVOmniConnectProxy.
   */
  void CopyFromObject(vtkObject* object) override;

  //@{
  /**
   * Manage a serialized version of the information.
   */
  void CopyToStream(vtkClientServerStream*) override;
  void CopyFromStream(const vtkClientServerStream*) override;

  const OmniConnectUrlInfoList& GetUrlInfoList() const { return UrlInfoList; }

protected:
  struct UrlStringsType
  {
    std::string Url;
    std::string Etag;
    std::string Author;
  };
  
  vtkPVOmniConnectProxyUrlInfo();
  ~vtkPVOmniConnectProxyUrlInfo() override;

  OmniConnectUrlInfoList UrlInfoList;

  std::vector<OmniConnectUrlInfo> UrlInfos;
  std::vector<UrlStringsType> UrlStrings;

private:
  vtkPVOmniConnectProxyUrlInfo(const vtkPVOmniConnectProxyUrlInfo&) = delete;
  void operator=(const vtkPVOmniConnectProxyUrlInfo&) = delete;
};

#endif
