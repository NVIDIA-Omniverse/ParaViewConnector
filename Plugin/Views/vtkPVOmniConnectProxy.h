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

#ifndef vtkPVOmniConnectProxy_h
#define vtkPVOmniConnectProxy_h

#include "OmniConnectViewsModule.h"
#include "vtkNew.h" // for vtkNew
#include "vtkObject.h"
#include "OmniConnectData.h"
#include "vtkOmniConnectPass.h"
#include "vtkPVOmniConnectProxyUrlInfo.h"

#include <string>

class OmniConnect;
class vtkSMProxy;

class OMNICONNECTVIEWS_EXPORT vtkPVOmniConnectProxy : public vtkObject
{
public:
  static vtkPVOmniConnectProxy* New();
  vtkTypeMacro(vtkPVOmniConnectProxy, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Client-side interface
  static void CreateConnection(vtkSMProxy* proxy
    , const OmniConnectSettings& settings
    , const OmniConnectEnvironment& environment);
  static void DestroyConnection(vtkSMProxy* proxy);

  static bool OpenConnection(vtkSMProxy* proxy, bool createSession);
  static void CancelOmniClientAuth(vtkSMProxy* proxy);

  static bool GetOmniClientEnabled(vtkSMProxy* proxy);
  static std::string GetClientVersion(vtkSMProxy* proxy);
  static int GetLatestSessionNumber(vtkSMProxy* proxy);
  static std::string GetUser(vtkSMProxy* proxy, const char* serverUrl);
  static void GetUrlInfoList(vtkSMProxy* proxy, const char* url, vtkPVOmniConnectProxyUrlInfo* proxyUrlInfo);

  static bool CreateFolder(vtkSMProxy* proxy, const char* url);

  // Server-side interface
  void CreateConnection_Server( 
    const char* omniServer
    , const char* omniWorkingDirectory
    , const char* localOutputDirectory
    , int outputLocal
    , int outputBinary
    , int upAxis
    , int usePointInstancer
    , int useMeshVolume
    , int createNewOmniSession
    , int procId
    , int numProcs
    );
  void DestroyConnection_Server();

  int OpenConnection_Server(int createSession);
  void CancelOmniClientAuth_Server();

  int GetOmniClientEnabled_Server();
  const char* GetClientVersion_Server();
  int GetLatestSessionNumber_Server();
  const char* GetUser_Server(const char* serverUrl);
  OmniConnectUrlInfoList GetUrlInfoList_Server(const char* url);

  bool CreateFolder_Server(const char* url);

  // Query arguments

  vtkSetStringMacro(UrlInfoList_Url);
  vtkGetStringMacro(UrlInfoList_Url);

  // Callbacks
  static void OmniClientAuthCallback(void* userData, bool show, const char* server, uint32_t authHandle) noexcept;

  static const char* OMNICONNECT_GROUP_NAME;
  static const char* OMNICONNECT_PROXY_NAME;

protected:
  vtkPVOmniConnectProxy();
  ~vtkPVOmniConnectProxy();

  void ResetInternals();

  bool IsClient = false;
  vtkOmniConnectSettings serverSettings;
  OmniConnect* Connector = nullptr;

  uint32_t LastAuthHandle = 0;

  char* UrlInfoList_Url = nullptr;

private:
  vtkPVOmniConnectProxy(const vtkPVOmniConnectProxy&); // Not implemented
  void operator=(const vtkPVOmniConnectProxy&); // Not implemented
};

#endif
