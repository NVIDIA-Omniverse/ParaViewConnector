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

#include "vtkPVOmniConnectProxy.h"

#include "vtkOmniConnectLogCallback.h"
#include "vtkOmniConnectVtkToOmni.h"
#include "OmniConnect.h"

#include "vtkPVSession.h"
#include "vtkProcessModule.h"
#include "vtkSMSession.h"
#include "vtkSMProxy.h"
#include "vtkClientServerStream.h"
#include "vtkSMStringVectorProperty.h"

#include <cassert>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkPVOmniConnectProxy);

//----------------------------------------------------------------------------
const char* vtkPVOmniConnectProxy::OMNICONNECT_GROUP_NAME = "OmniConnectProxyGroup";
const char* vtkPVOmniConnectProxy::OMNICONNECT_PROXY_NAME = "OmniConnectProxy";

//----------------------------------------------------------------------------
vtkPVOmniConnectProxy::vtkPVOmniConnectProxy()
{
  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  if (pm->GetPartitionId() > 0)
  {
    return;
  }

  vtkPVSession* session = vtkPVSession::SafeDownCast(pm->GetSession());
  if (session && !session->HasProcessRole(vtkPVSession::RENDER_SERVER))
  {
    IsClient = true;
  }
}

//----------------------------------------------------------------------------
vtkPVOmniConnectProxy::~vtkPVOmniConnectProxy()
{
  if(Connector)
    DestroyConnection_Server();
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectProxy::CreateConnection(vtkSMProxy* proxy
  , const OmniConnectSettings& settings
  , const OmniConnectEnvironment& environment)
{
  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke << VTKOBJECT(proxy) << "CreateConnection_Server"
         << settings.OmniServer << settings.OmniWorkingDirectory << settings.LocalOutputDirectory 
         << (int)settings.OutputLocal
         << (int)settings.OutputBinary
         << (int)settings.UpAxis
         << (int)settings.UsePointInstancer
         << (int)settings.UseMeshVolume
         << (int)settings.CreateNewOmniSession
         << environment.ProcId
         << environment.NumProcs
         << vtkClientServerStream::End;
  vtkSMSession* session = proxy->GetSession();
  session->ExecuteStream(vtkPVSession::SERVERS, stream, false);
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectProxy::DestroyConnection(vtkSMProxy* proxy)
{
  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke << VTKOBJECT(proxy) << "DestroyConnection_Server"
         << vtkClientServerStream::End;
  vtkSMSession* session = proxy->GetSession();
  session->ExecuteStream(vtkPVSession::SERVERS, stream, false);
}

//----------------------------------------------------------------------------
bool vtkPVOmniConnectProxy::OpenConnection(vtkSMProxy* proxy, bool createSession)
{
  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke << VTKOBJECT(proxy) << "OpenConnection_Server"
         << (int) createSession
         << vtkClientServerStream::End;
  vtkSMSession* session = proxy->GetSession();
  session->ExecuteStream(vtkPVSession::SERVERS, stream, false);

  vtkClientServerStream result = session->GetLastResult(vtkPVSession::SERVERS);
  int connectionEstablished = 0;
  if (result.GetNumberOfMessages() == 1 && result.GetNumberOfArguments(0) == 1)
  {
    result.GetArgument(0, 0, &connectionEstablished);
  }

  return connectionEstablished;
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectProxy::CancelOmniClientAuth(vtkSMProxy* proxy)
{
  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke << VTKOBJECT(proxy) << "CancelOmniClientAuth_Server"
         << vtkClientServerStream::End;
  vtkSMSession* session = proxy->GetSession();
  session->ExecuteStream(vtkPVSession::SERVERS, stream, false);
}

//----------------------------------------------------------------------------
bool vtkPVOmniConnectProxy::GetOmniClientEnabled(vtkSMProxy* proxy)
{
  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke << VTKOBJECT(proxy) << "GetOmniClientEnabled_Server"
         << vtkClientServerStream::End;
  vtkSMSession* session = proxy->GetSession();
  session->ExecuteStream(vtkPVSession::SERVERS, stream, false);

  vtkClientServerStream result = session->GetLastResult(vtkPVSession::SERVERS);
  int clientEnabled = 0;
  if (result.GetNumberOfMessages() == 1 && result.GetNumberOfArguments(0) == 1)
  {
    result.GetArgument(0, 0, &clientEnabled);
  }

  return clientEnabled;
}

//----------------------------------------------------------------------------
std::string vtkPVOmniConnectProxy::GetClientVersion(vtkSMProxy* proxy)
{
  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke << VTKOBJECT(proxy) << "GetClientVersion_Server"
         << vtkClientServerStream::End;
  vtkSMSession* session = proxy->GetSession();
  session->ExecuteStream(vtkPVSession::SERVERS, stream, false);

  vtkClientServerStream result = session->GetLastResult(vtkPVSession::SERVERS);
  std::string clientVersion;
  if (result.GetNumberOfMessages() == 1 && result.GetNumberOfArguments(0) == 1)
  {
    result.GetArgument(0, 0, &clientVersion);
  }

  return clientVersion;
}

//----------------------------------------------------------------------------
int vtkPVOmniConnectProxy::GetLatestSessionNumber(vtkSMProxy* proxy)
{
  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke << VTKOBJECT(proxy) << "GetLatestSessionNumber_Server"
         << vtkClientServerStream::End;
  vtkSMSession* session = proxy->GetSession();
  session->ExecuteStream(vtkPVSession::SERVERS, stream, false);

  vtkClientServerStream result = session->GetLastResult(vtkPVSession::SERVERS);
  int sessionNr = 0;
  if (result.GetNumberOfMessages() == 1 && result.GetNumberOfArguments(0) == 1)
  {
    result.GetArgument(0, 0, &sessionNr);
  }

  return sessionNr;
}

//----------------------------------------------------------------------------
std::string vtkPVOmniConnectProxy::GetUser(vtkSMProxy* proxy, const char* serverUrl)
{
  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke << VTKOBJECT(proxy) << "GetUser_Server"
         << serverUrl
         << vtkClientServerStream::End;
  vtkSMSession* session = proxy->GetSession();
  session->ExecuteStream(vtkPVSession::SERVERS, stream, false);

  vtkClientServerStream result = session->GetLastResult(vtkPVSession::SERVERS);
  std::string userName;
  if (result.GetNumberOfMessages() == 1 && result.GetNumberOfArguments(0) == 1)
  {
    result.GetArgument(0, 0, &userName);
  }

  return userName;
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectProxy::GetUrlInfoList(vtkSMProxy* proxy, const char* url, vtkPVOmniConnectProxyUrlInfo* proxyUrlInfo)
{
  vtkSMProperty* prop = proxy->GetProperty("UrlInfoList_Url");
  assert(prop);
  vtkSMStringVectorProperty* vectorProp = vtkSMStringVectorProperty::SafeDownCast(prop);
  assert(vectorProp != nullptr);
  vectorProp->SetElement(0, url);
  proxy->UpdateVTKObjects();

  proxy->GatherInformation(proxyUrlInfo);
}

//----------------------------------------------------------------------------
bool vtkPVOmniConnectProxy::CreateFolder(vtkSMProxy* proxy, const char* url)
{
  vtkClientServerStream stream;
  stream << vtkClientServerStream::Invoke << VTKOBJECT(proxy) << "CreateFolder_Server"
         << url
         << vtkClientServerStream::End;
  vtkSMSession* session = proxy->GetSession();
  session->ExecuteStream(vtkPVSession::SERVERS, stream, false);

  vtkClientServerStream result = session->GetLastResult(vtkPVSession::SERVERS);
  int success = 0;
  if (result.GetNumberOfMessages() == 1 && result.GetNumberOfArguments(0) == 1)
  {
    result.GetArgument(0, 0, &success);
  }

  return success;
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectProxy::CreateConnection_Server(
  const char* omniServer
  , const char* omniWorkingDirectory
  , const char* localOutputDirectory
  , int outputLocal // bools seems to generate runtime errors
  , int outputBinary
  , int upAxis
  , int usePointInstancer
  , int useMeshVolume
  , int createNewOmniSession
  , int procId
  , int numProcs
  )
{
  if(Connector)
    DestroyConnection_Server();

  serverSettings.OmniServer = omniServer;
  serverSettings.OmniWorkingDirectory = omniWorkingDirectory;
  serverSettings.LocalOutputDirectory = localOutputDirectory;
  serverSettings.OutputLocal = outputLocal;
  serverSettings.OutputBinary = outputBinary;
  serverSettings.UpAxis = (upAxis == 0) ? OmniConnectAxis::X : ((upAxis == 1) ? OmniConnectAxis::Y : OmniConnectAxis::Z);
  serverSettings.UsePointInstancer = usePointInstancer;
  serverSettings.UseMeshVolume = useMeshVolume;
  serverSettings.CreateNewOmniSession = createNewOmniSession;

  OmniConnectSettings connectSettings;
  GetOmniConnectSettings(serverSettings, connectSettings);
  
  OmniConnectEnvironment environment;
  environment.ProcId = procId;
  environment.NumProcs = numProcs;
 
  Connector = new OmniConnect(connectSettings, environment, vtkOmniConnectLogCallback::Callback);
  Connector->SetAuthMessageBoxCallback(this, OmniClientAuthCallback);
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectProxy::DestroyConnection_Server()
{
  delete Connector;
  ResetInternals();
}

//----------------------------------------------------------------------------
int vtkPVOmniConnectProxy::OpenConnection_Server(int createSession)
{
  if(Connector)
  {
    return Connector->OpenConnection(createSession);
  }
  return 0;
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectProxy::CancelOmniClientAuth_Server()
{
  if(Connector && LastAuthHandle != 0)
    Connector->CancelOmniClientAuth(LastAuthHandle);
}

//----------------------------------------------------------------------------
int vtkPVOmniConnectProxy::GetOmniClientEnabled_Server()
{
  return OmniConnect::OmniClientEnabled();
}

//----------------------------------------------------------------------------
const char* vtkPVOmniConnectProxy::GetClientVersion_Server()
{
  if(Connector)
    return Connector->GetClientVersion();

  return nullptr;
}

//----------------------------------------------------------------------------
int vtkPVOmniConnectProxy::GetLatestSessionNumber_Server()
{
  if(Connector)
    return Connector->GetLatestSessionNumber();

  return 0;
}

//----------------------------------------------------------------------------
const char* vtkPVOmniConnectProxy::GetUser_Server(const char* serverUrl)
{
  if(Connector)
    return Connector->GetUser(serverUrl);

  return nullptr;
}

OmniConnectUrlInfoList vtkPVOmniConnectProxy::GetUrlInfoList_Server(const char* url)
{
  OmniConnectUrlInfoList urlInfoList;
  if(Connector)
    urlInfoList = Connector->GetUrlInfoList(url);

  return urlInfoList;
}

//----------------------------------------------------------------------------
bool vtkPVOmniConnectProxy::CreateFolder_Server(const char* url)
{
  if(Connector)
    return Connector->CreateFolder(url);

  return false;
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectProxy::ResetInternals()
{
  Connector = nullptr;
  LastAuthHandle = 0;
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectProxy::OmniClientAuthCallback(void* userData, bool show, const char* server, uint32_t authHandle) noexcept 
{
  vtkPVOmniConnectProxy* connectProxy = reinterpret_cast<vtkPVOmniConnectProxy*>(userData);
  connectProxy->LastAuthHandle = authHandle;
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectProxy::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}



