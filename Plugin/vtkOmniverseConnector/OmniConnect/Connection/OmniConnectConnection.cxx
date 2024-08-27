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

#include "OmniConnectConnection.h"

#include <fstream>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <algorithm>
#include <cstring>
#include <vector>

#ifdef _WIN32
#include <filesystem>
namespace fs = std::filesystem;
#else
#if __GNUC__ < 8
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <filesystem>
namespace fs = std::filesystem;
#endif
#endif

#define OmniConnectLogMacro(level, message) \
  { std::stringstream logStream; \
    logStream << message; \
    std::string logString = logStream.str(); \
    try \
    { \
      OmniConnectConnection::LogCallback(level, OmniConnectConnection::LogUserData, logString.c_str()); \
    } \
    catch (...) {} \
  }


#define CONNECT_CATCH(a)                                                       \
  catch (const std::bad_alloc &)                                               \
  {                                                                            \
    OmniConnectLogMacro(OmniConnectLogLevel::ERR, "OMNICONNECTCONNECTION BAD ALLOC\n");                  \
    return a;                                                                  \
  }                                                                            \
  catch (const std::exception &e)                                              \
  {                                                                            \
    OmniConnectLogMacro(OmniConnectLogLevel::ERR, "OMNICONNECTCONNECTION ERROR: " << e.what());          \
    return a;                                                                  \
  }                                                                            \
  catch (...)                                                                  \
  {                                                                            \
    OmniConnectLogMacro(OmniConnectLogLevel::ERR, "OMNICONNECTCONNECTION UNKOWN EXCEPTION\n");           \
    return a;                                                                  \
  }

OmniConnectLogCallback OmniConnectConnection::LogCallback = nullptr;
void* OmniConnectConnection::LogUserData = nullptr;

const char* OmniConnectConnection::GetBaseUrl() const
{
  return Settings.WorkingDirectory.c_str();
}

const char* OmniConnectConnection::GetUrl(const char* path) const
{
  TempUrl = Settings.WorkingDirectory + path;
  return TempUrl.c_str();
}

bool OmniConnectConnection::Initialize(const OmniConnectConnectionSettings& settings, 
  OmniConnectLogCallback logCallback, void* logUserData)
{
  Settings = settings;
  OmniConnectConnection::LogCallback = logCallback;
  OmniConnectConnection::LogUserData = logUserData;
  return true;
}

void OmniConnectConnection::Shutdown()
{
}

int OmniConnectConnection::MaxSessionNr() const
{
  int sessionNr = 0;
  try
  {
    for (;; ++sessionNr)
    {
      TempUrl = Settings.WorkingDirectory + "Session_" + std::to_string(sessionNr);
      if (!fs::exists(TempUrl))
        break;
    }
  }
  CONNECT_CATCH(-1)

  return sessionNr - 1;
}

bool OmniConnectConnection::CreateFolder(const char* dirName, bool mayExist, bool combineBaseUrl) const
{
  try
  {
    TempUrl = combineBaseUrl ? Settings.WorkingDirectory + dirName : dirName;
    if (!mayExist || !fs::exists(TempUrl))
      return fs::create_directory(TempUrl);
    else
      return true;
  }
  CONNECT_CATCH(false)
}

bool OmniConnectConnection::RemoveFolder(const char* dirName) const
{
  bool success = false;
  try
  {
    TempUrl = Settings.WorkingDirectory + dirName;
    if (fs::exists(TempUrl))
      success = fs::remove_all(TempUrl);
  }
  CONNECT_CATCH(false)

  return success;
}

bool OmniConnectConnection::WriteFile(const char* data, size_t dataSize, const char* filePath, bool binary) const
{
  try
  {
    std::ofstream file(Settings.WorkingDirectory + filePath, std::ios_base::out
      | std::ios_base::trunc
      | (binary ? std::ios_base::binary : std::ios_base::out));
    if (file.is_open())
    {
      file.write(data, dataSize);
      file.close();
      return true;
    }
  }
  CONNECT_CATCH(false)

  return false;
}

bool OmniConnectConnection::RemoveFile(const char* filePath) const
{
  bool success = false;
  try
  {
    TempUrl = Settings.WorkingDirectory + filePath;
    if (fs::exists(TempUrl))
      success = fs::remove(TempUrl);
  }
  CONNECT_CATCH(false)

  return success;
}

bool OmniConnectConnection::ProcessUpdates()
{
  return true;
}

const char* OmniConnectConnection::GetOmniUser(const char*) const
{
  return "Guest";
}

OmniConnectUrlInfoList OmniConnectConnection::GetOmniConnectUrlInfoList(const char*)
{
  // Only implemented in OmniConnectRemoteConnection with OMNIVERSE_CONNECTION_ENABLE turned on.
  return OmniConnectUrlInfoList();
}

const char* OmniConnectConnection::GetOmniClientVersion()
{
  return "";
}

void OmniConnectConnection::SetAuthMessageBoxCallback(void*, OmniConnectAuthCallback)
{
  // Only implemented in OmniConnectRemoteConnection
}

void OmniConnectConnection::CancelOmniClientAuth(uint32_t authHandle)
{
  // Only implemented in OmniConnectRemoteConnection
}

class OmniConnectRemoteConnectionInternals
{
public:
  struct UrlStringsType
  {
    std::string Url;
    std::string Etag;
    std::string Author;
  };
  
  OmniConnectRemoteConnectionInternals()
  {}
  ~OmniConnectRemoteConnectionInternals()
  {
    delete RemoteStream;
  }

  std::ostream* ResetStream(const char * filePath, bool binary)
  {
    StreamFilePath = filePath;
    StreamIsBinary = binary;
    if (RemoteStream)
      delete RemoteStream;
    RemoteStream = new std::stringstream(std::ios::in | std::ios::out);
    return RemoteStream;
  }

  // To facility the Omniverse path methods
  static constexpr size_t MaxBaseUrlSize = 4096;
  char BaseUrlBuffer[MaxBaseUrlSize];
  char TempUrlBuffer[MaxBaseUrlSize];

  // Status callback handle
  uint32_t StatusCallbackHandle = 0;

  // Connection info
  std::vector<OmniConnectUrlInfo> UrlInfos;
  std::vector<UrlStringsType> UrlStrings;
  std::string OmniUser;
  std::string OmniClientVersion;

  // Stream
  const char* StreamFilePath;
  bool StreamIsBinary;
  std::stringstream* RemoteStream = nullptr;
};

using OmniConnectUrlStrings = OmniConnectRemoteConnectionInternals::UrlStringsType;

bool OmniConnectRemoteConnection::OmniClientInitialized = false;
int OmniConnectRemoteConnection::NumInitializedConnInstances = 0;
int OmniConnectRemoteConnection::ConnectionLogLevel = 0;

OmniConnectRemoteConnection::OmniConnectRemoteConnection()
  : Internals(new OmniConnectRemoteConnectionInternals())
{
}

OmniConnectRemoteConnection::~OmniConnectRemoteConnection()
{
  delete Internals;
}

#ifdef OMNIVERSE_CONNECTION_ENABLE

#include <OmniClient.h>

namespace
{
  struct DefaultContext
  {
    OmniClientResult result = eOmniClientResult_Error;
    bool done = false;
  };

  const char* omniResultToString(OmniClientResult result)
  {
    const char* msg = nullptr;
    switch (result)
    {
      case eOmniClientResult_Ok: msg = "eOmniClientResult_Ok"; break;
      case eOmniClientResult_OkLatest: msg = "eOmniClientResult_OkLatest"; break;
      case eOmniClientResult_OkNotYetFound: msg = "eOmniClientResult_OkNotYetFound"; break;
      case eOmniClientResult_Error: msg = "eOmniClientResult_Error"; break;
      case eOmniClientResult_ErrorConnection: msg = "eOmniClientResult_ErrorConnection"; break;
      case eOmniClientResult_ErrorNotSupported: msg = "eOmniClientResult_ErrorNotSupported"; break;
      case eOmniClientResult_ErrorAccessDenied: msg = "eOmniClientResult_ErrorAccessDenied"; break;
      case eOmniClientResult_ErrorNotFound: msg = "eOmniClientResult_ErrorNotFound"; break;
      case eOmniClientResult_ErrorBadVersion: msg = "eOmniClientResult_ErrorBadVersion"; break;
      case eOmniClientResult_ErrorAlreadyExists: msg = "eOmniClientResult_ErrorAlreadyExists"; break;
      case eOmniClientResult_ErrorAccessLost: msg = "eOmniClientResult_ErrorAccessLost"; break;
      default: msg = "Unknown OmniClientResult"; break;
    };
    return msg;
  }

  void OmniClientStatusCB(const char* threadName, const char* component, OmniClientLogLevel level, const char* message) OMNICLIENT_NOEXCEPT
  {
    static std::mutex statusMutex;
    std::unique_lock<std::mutex> lk(statusMutex);

    // This will return "Verbose", "Info", "Warning", "Error"
    //const char* levelString = omniClientGetLogLevelString(level);
    char levelChar = omniClientGetLogLevelChar(level);

    OmniConnectLogMacro(OmniConnectLogLevel::STATUS, "OmniClient status message: ");
    OmniConnectLogMacro(OmniConnectLogLevel::STATUS, "level: " << levelChar << ", thread: " << threadName << ", component: " << component << ", msg: " << message);
  }

  void OmniClientConnectionStatusCB(void* userData, char const* url, OmniClientConnectionStatus status) OMNICLIENT_NOEXCEPT
  {
    (void)userData;

    static std::mutex statusMutex;
    std::unique_lock<std::mutex> lk(statusMutex);

    {
      std::string urlStr;
      if (url) urlStr = url;
      switch (status)
      {
        case eOmniClientConnectionStatus_Connecting: { OmniConnectLogMacro(OmniConnectLogLevel::STATUS, "Attempting to connect to " << urlStr); break; }
        case eOmniClientConnectionStatus_Connected: { OmniConnectLogMacro(OmniConnectLogLevel::STATUS, "Successfully connected to " << urlStr); break; }
        case eOmniClientConnectionStatus_ConnectError: { OmniConnectLogMacro(OmniConnectLogLevel::STATUS, "Error trying to connect (will attempt reconnection) to " << urlStr); break; }
        case eOmniClientConnectionStatus_Disconnected: { OmniConnectLogMacro(OmniConnectLogLevel::STATUS, "Disconnected (will attempt reconnection) to " << urlStr); break; }
        case Count_eOmniClientConnectionStatus:
        default:
          break;
      }
    }
  }

  OmniClientResult IsValidServerConnection(const char* serverUrl)
  {
    struct ServerInfoContect : public DefaultContext
    {
      std::string retVersion;
    } context;

    omniClientWait( omniClientGetServerInfo(serverUrl, &context,
      [](void* userData, OmniClientResult result, OmniClientServerInfo const * info) OMNICLIENT_NOEXCEPT
      {
        ServerInfoContect& context = *(ServerInfoContect*)(userData);

        if (context.result != eOmniClientResult_Ok)
        {
          context.result = result;
          if (result == eOmniClientResult_Ok && info->version)
          {
            context.retVersion = info->version;
          }
          context.done = true;
        }
      }));

    return context.result;
  }

  void OmniConnectSetConnectionLogLevel(int logLevel)
  {
    int clampedLevel = eOmniClientLogLevel_Debug + (logLevel < 0 ? 0 : logLevel);
    clampedLevel = (clampedLevel < (int)Count_eOmniClientLogLevel) ? clampedLevel : Count_eOmniClientLogLevel - 1;
    omniClientSetLogLevel((OmniClientLogLevel)clampedLevel);
  }
}

const char* OmniConnectRemoteConnection::GetBaseUrl() const
{
  return Internals->BaseUrlBuffer;
}

const char* OmniConnectRemoteConnection::GetUrl(const char* path) const
{
  size_t parsedBufSize = Internals->MaxBaseUrlSize;
  char* combinedUrl = omniClientCombineUrls(Internals->BaseUrlBuffer, path, Internals->TempUrlBuffer, &parsedBufSize);

  return combinedUrl;
}

bool OmniConnectRemoteConnection::Initialize(const OmniConnectConnectionSettings& settings, 
  OmniConnectLogCallback logCallback, void* logUserData)
{
  bool initSuccess = OmniConnectConnection::Initialize(settings, logCallback, logUserData);

  if (NumInitializedConnInstances == 0)
  {
    omniClientSetLogCallback(OmniClientStatusCB);
  }

  if (initSuccess && !OmniClientInitialized)
  {
    OmniConnectLogMacro(OmniConnectLogLevel::STATUS, "Establishing connection - Omniverse Client Initialization -");

    initSuccess = OmniClientInitialized = omniClientInitialize(kOmniClientVersion);
    OmniConnectSetConnectionLogLevel(ConnectionLogLevel);
  }

  if (initSuccess && NumInitializedConnInstances == 0)
  {
    Internals->StatusCallbackHandle = omniClientRegisterConnectionStatusCallback(nullptr, OmniClientConnectionStatusCB);
  }

  if (initSuccess)
  {
    // Check for Url validity for its various components
    std::string serverUrl = "omniverse://" + Settings.HostName + "/";
    std::string rawUrl = serverUrl + Settings.WorkingDirectory;
    OmniClientUrl* brokenUrl = omniClientBreakUrl(rawUrl.c_str());

    if (!brokenUrl->scheme)
    {
      OmniConnectLogMacro(OmniConnectLogLevel::ERR, "Illegal Omniverse scheme.");
      initSuccess = false;
    }

    if (!brokenUrl->host || strlen(brokenUrl->host) == 0)
    {
      OmniConnectLogMacro(OmniConnectLogLevel::ERR, "Illegal Omniverse server.");
      initSuccess = false;
    }

    if (!brokenUrl->port || strlen(brokenUrl->port) == 0)
      OmniConnectLogMacro(OmniConnectLogLevel::STATUS, "Warning: No port specified for Omniverse server");

    if (!brokenUrl->path || strlen(brokenUrl->path) == 0)
    {
      OmniConnectLogMacro(OmniConnectLogLevel::ERR, "Illegal Omniverse working directory.");
      initSuccess = false;
    }

    char* urlRes = nullptr;
    if (initSuccess)
    {
      size_t parsedBufSize = Internals->MaxBaseUrlSize;
      urlRes = omniClientMakeUrl(brokenUrl, Internals->BaseUrlBuffer, &parsedBufSize);
      if (!urlRes)
      {
        OmniConnectLogMacro(OmniConnectLogLevel::ERR, "Connection Url invalid, exceeds 4096 characters.");
        initSuccess = false;
      }
    }

    if (initSuccess)
    {
      OmniClientResult result = IsValidServerConnection(serverUrl.c_str());
      if (result != eOmniClientResult_Ok)
      {
        OmniConnectLogMacro(OmniConnectLogLevel::ERR, "Server connection cannot be established, errorcode:" << omniResultToString(result));
        initSuccess = false;
      }
    }

    if (initSuccess && settings.CheckWritePermissions)
    {
      bool result = this->CheckWritePermissions();
      if (!result)
      {
        OmniConnectLogMacro(OmniConnectLogLevel::ERR, "Omniverse user does not have write permissions to selected output directory.");
        initSuccess = false;
      }
    }

    if (initSuccess)
    {
      OmniConnectLogMacro(OmniConnectLogLevel::STATUS, "Server for connection:" << brokenUrl->host);
      if (brokenUrl->port)
      {
        OmniConnectLogMacro(OmniConnectLogLevel::STATUS, "Port for connection:" << brokenUrl->port);
      }

      if (NumInitializedConnInstances == 0)
      {
        //omniClientPushBaseUrl(urlRes); // No base url, since multiple connection to multiple location may be created
      }

      ++NumInitializedConnInstances;
      ConnectionInitialized = true;
    }

    omniClientFreeUrl(brokenUrl);
  }

  return initSuccess;
}

void OmniConnectRemoteConnection::Shutdown()
{
  if (ConnectionInitialized && --NumInitializedConnInstances == 0)
  {
    omniClientSetLogCallback(nullptr);
    if (Internals->StatusCallbackHandle)
      omniClientUnregisterCallback(Internals->StatusCallbackHandle);

    omniClientLiveWaitForPendingUpdates();

    //omniClientPopBaseUrl(Internals->BaseUrlBuffer);

    //omniClientShutdown();
  }
}

int OmniConnectRemoteConnection::MaxSessionNr() const
{
  struct SessionListContext : public DefaultContext
  {
    int sessionNumber = -1;
  } context;
  
  const char* baseUrl = this->GetBaseUrl();
  
  omniClientWait( omniClientList(
    baseUrl, &context,
    [](void* userData, OmniClientResult result, uint32_t numEntries, struct OmniClientListEntry const* entries) OMNICLIENT_NOEXCEPT
    {
      auto& context = *(SessionListContext*)(userData);
      {
        if (result == eOmniClientResult_Ok)
        {
          for (uint32_t i = 0; i < numEntries; i++)
          {
            const char* relPath = entries[i].relativePath;
            if (strncmp("Session_", relPath, 8) == 0)
            {
              int pathSessionNr = std::atoi(relPath + 8);

              context.sessionNumber = std::max(context.sessionNumber, pathSessionNr);
            }
          }
        }
        context.done = true;
      }
    })
  );

  return context.sessionNumber;
}

bool OmniConnectRemoteConnection::CreateFolder(const char* dirName, bool mayExist, bool combineBaseUrl) const
{
  DefaultContext context;

  const char* dirUrl = combineBaseUrl ? this->GetUrl(dirName) : dirName;
  omniClientWait( omniClientCreateFolder(dirUrl, &context,
    [](void* userData, OmniClientResult result) OMNICLIENT_NOEXCEPT
    {
      auto& context = *(DefaultContext*)(userData);
      context.result = result;
      context.done = true;
    })
  );

  return context.result == eOmniClientResult_Ok || context.result == eOmniClientResult_OkLatest 
    || (mayExist && context.result == eOmniClientResult_ErrorAlreadyExists);
}

bool OmniConnectRemoteConnection::RemoveFolder(const char* dirName) const
{
  DefaultContext context;

  const char* dirUrl = this->GetUrl(dirName);
  omniClientWait( omniClientDelete(dirUrl, &context,
    [](void* userData, OmniClientResult result) OMNICLIENT_NOEXCEPT
    {
      auto& context = *(DefaultContext*)(userData);
      context.result = result;
      context.done = true;
    })
  );

  return context.result == eOmniClientResult_Ok || context.result == eOmniClientResult_OkLatest;
}

bool OmniConnectRemoteConnection::WriteFile(const char* data, size_t dataSize, const char* filePath, bool binary) const
{
  (void)binary;
  OmniConnectLogMacro(OmniConnectLogLevel::STATUS, "Copying data to: " << filePath);

  DefaultContext context;

  const char* fileUrl = this->GetUrl(filePath);
  OmniClientContent omniContent{ (void*)data, dataSize, nullptr };
  omniClientWait( omniClientWriteFile(fileUrl, &omniContent, &context, [](void* userData, OmniClientResult result) OMNICLIENT_NOEXCEPT
    {
      auto& context = *(DefaultContext*)(userData);
      context.result = result;
      context.done = true;
    })
  );

  return context.result == eOmniClientResult_Ok || context.result == eOmniClientResult_OkLatest;
}

bool OmniConnectRemoteConnection::RemoveFile(const char* filePath) const
{
  DefaultContext context;
  OmniConnectLogMacro(OmniConnectLogLevel::STATUS, "Removing file: " << filePath);

  const char* fileUrl = this->GetUrl(filePath);
  omniClientWait(omniClientDelete(fileUrl, &context, [](void* userData, OmniClientResult result) OMNICLIENT_NOEXCEPT
    {
      auto& context = *(DefaultContext*)(userData);
      context.result = result;
      context.done = true;
    })
  );

  return context.result == eOmniClientResult_Ok || context.result == eOmniClientResult_OkLatest;
}

bool OmniConnectRemoteConnection::ProcessUpdates()
{
  omniClientLiveProcess();
  //omniClientLiveWaitForPendingUpdates(); // Refactor: test whether this should be enabled

  return true;
}

const char* OmniConnectRemoteConnection::GetOmniUser(const char* serverUrl) const
{
  struct UserInfoContext
  {
    OmniClientResult result = eOmniClientResult_Error;
    bool done = false;
    OmniConnectRemoteConnectionInternals* internals;
  } context;

  context.internals = Internals;

  omniClientWait(omniClientGetServerInfo(serverUrl, &context,
    [](void* userData, OmniClientResult result, struct OmniClientServerInfo const * info) OMNICLIENT_NOEXCEPT
    {
      UserInfoContext& context = *(UserInfoContext*)(userData);

      if (context.result != eOmniClientResult_Ok)
      {
        context.result = result;
        if (result == eOmniClientResult_Ok && info)
        {
          context.internals->OmniUser = info->username;
        }
        else
        {
          context.internals->OmniUser = "Guest";
        }
        context.done = true;
      }
    })
  );
  return Internals->OmniUser.c_str();
}

OmniConnectUrlInfoList OmniConnectRemoteConnection::GetOmniConnectUrlInfoList(const char* serverUrl) 
{
  struct Context
  {
    std::mutex mutex;
    std::condition_variable cv;
    OmniConnectRemoteConnectionInternals* internals;
    bool found = false;
    bool done = false;
  } context;
  std::unique_lock<std::mutex> lock(context.mutex);

  context.internals = Internals;

  Internals->UrlStrings.resize(0);
  Internals->UrlInfos.resize(0);
  
  // Ask for the list of files/folders in Omniverse
  omniClientList(serverUrl, &context,
    [](void* userData, OmniClientResult result, uint32_t numEntries, struct OmniClientListEntry const* entries) OMNICLIENT_NOEXCEPT
    {
      auto& context = *(Context*)(userData);
      {
        std::unique_lock<std::mutex> lock(context.mutex);
        if (result == eOmniClientResult_Ok)
        {
          for (uint32_t i = 0; i < numEntries; i++)
          {
            std::string url (entries[i].relativePath);
            bool unwantedUrl = (url.size() < 1) || (url.find("/.") != std::string::npos) || (url.find("/@") != std::string::npos) || (url.find("/TeamCity/") != std::string::npos) || (url.find(".sublayer.") != std::string::npos);
            if (!unwantedUrl)
            {
              OmniConnectUrlStrings urlStrings;
              urlStrings.Url = entries[i].relativePath;
              urlStrings.Author = (entries[i].modifiedBy == nullptr) ? "" : entries[i].modifiedBy;
              urlStrings.Etag;

              // skip some system files
              OmniConnectUrlInfo localInfo;
              localInfo.ModifiedTime = (time_t)(entries[i].modifiedTimeNs / 1'000'000'000);
              localInfo.Size = entries[i].size;
              localInfo.IsFile = false;
              if (entries[i].flags & fOmniClientItem_ReadableFile)
              {
                localInfo.IsFile = true;
              }
              context.internals->UrlStrings.push_back(std::move(urlStrings));
              context.internals->UrlInfos.push_back(localInfo);
            }
          }
        }
        context.done = true;
      }
      context.cv.notify_all();
    });

  // Wait until all of the data is retrieved
  while (!context.done)
  {
    context.cv.wait(lock);
  }

  // Copy over string pointers *after* lists have been constructed 
  for(size_t i = 0; i < Internals->UrlInfos.size(); ++i)
  {
    OmniConnectUrlStrings& urlStrings = Internals->UrlStrings[i];
    OmniConnectUrlInfo& urlInfo = Internals->UrlInfos[i];

    urlInfo.Url = urlStrings.Url.c_str();
    urlInfo.Author = urlStrings.Author.c_str();
    urlInfo.Etag = urlStrings.Etag.c_str();
  }

  OmniConnectUrlInfoList infoList = { Internals->UrlInfos.data(), Internals->UrlInfos.size() };
  return infoList;
}

const char* OmniConnectRemoteConnection::GetOmniClientVersion() 
{
  Internals->OmniClientVersion = omniClientGetVersionString();
  return Internals->OmniClientVersion.c_str();
}

void OmniConnectRemoteConnection::SetAuthMessageBoxCallback(void* userData, OmniConnectAuthCallback callback)
{
  omniClientSetAuthenticationMessageBoxCallback(userData, callback);
}

void OmniConnectRemoteConnection::CancelOmniClientAuth(uint32_t authHandle)
{
  omniClientAuthenticationCancel(authHandle);
}

bool OmniConnectRemoteConnection::CheckWritePermissions()
{
  bool success = this->CreateFolder("", true);
  return success;
}

void OmniConnectRemoteConnection::SetConnectionLogLevel(int logLevel)
{
  ConnectionLogLevel = logLevel;
  if (OmniClientInitialized)
  {
    OmniConnectSetConnectionLogLevel(logLevel);
  }
}

int OmniConnectRemoteConnection::GetConnectionLogLevelMax()
{
  return Count_eOmniClientLogLevel - 1;
}

#else

const char* OmniConnectRemoteConnection::GetBaseUrl() const
{
  return OmniConnectConnection::GetBaseUrl();
}

const char* OmniConnectRemoteConnection::GetUrl(const char* path) const
{
  return OmniConnectConnection::GetUrl(path);
}

bool OmniConnectRemoteConnection::Initialize(const OmniConnectConnectionSettings& settings, 
  OmniConnectLogCallback logCallback, void* logUserData)
{
  return OmniConnectConnection::Initialize(settings, logCallback, logUserData);
}

void OmniConnectRemoteConnection::Shutdown()
{
  OmniConnectConnection::Shutdown();
}

int OmniConnectRemoteConnection::MaxSessionNr() const
{
  return OmniConnectConnection::MaxSessionNr();
}

bool OmniConnectRemoteConnection::CreateFolder(const char* dirName, bool mayExist, bool combineBaseUrl) const
{
  return OmniConnectConnection::CreateFolder(dirName, mayExist, combineBaseUrl);
}

bool OmniConnectRemoteConnection::RemoveFolder(const char* dirName) const
{
  return OmniConnectConnection::RemoveFolder(dirName);
}

bool OmniConnectRemoteConnection::WriteFile(const char* data, size_t dataSize, const char* filePath, bool binary) const
{
  return OmniConnectConnection::WriteFile(data, dataSize, filePath, binary);
}

bool OmniConnectRemoteConnection::RemoveFile(const char* filePath) const
{
  return OmniConnectConnection::RemoveFile(filePath);
}

bool OmniConnectRemoteConnection::ProcessUpdates()
{
  OmniConnectConnection::ProcessUpdates();

  return true;
}

const char* OmniConnectRemoteConnection::GetOmniUser(const char* serverUrl) const
{
  return OmniConnectConnection::GetOmniUser(serverUrl);
}

OmniConnectUrlInfoList OmniConnectRemoteConnection::GetOmniConnectUrlInfoList(const char* serverUrl)
{
  return OmniConnectConnection::GetOmniConnectUrlInfoList(serverUrl);
}

const char* OmniConnectRemoteConnection::GetOmniClientVersion()
{
  return OmniConnectConnection::GetOmniClientVersion();
}

void OmniConnectRemoteConnection::SetAuthMessageBoxCallback(void* userData, OmniConnectAuthCallback callback)
{
  OmniConnectConnection::SetAuthMessageBoxCallback(userData, callback);
}

void OmniConnectRemoteConnection::CancelOmniClientAuth(uint32_t authHandle)
{
  OmniConnectConnection::CancelOmniClientAuth(authHandle);
}

bool OmniConnectRemoteConnection::CheckWritePermissions()
{
  return true; 
}

void OmniConnectRemoteConnection::SetConnectionLogLevel(int logLevel)
{
}

int OmniConnectRemoteConnection::GetConnectionLogLevelMax()
{
  return 0;
}

#endif //OMNIVERSE_CONNECTION_ENABLE


OmniConnectLocalConnection::OmniConnectLocalConnection()
{
}

OmniConnectLocalConnection::~OmniConnectLocalConnection()
{
}

const char* OmniConnectLocalConnection::GetBaseUrl() const
{
  return OmniConnectConnection::GetBaseUrl();
}

const char* OmniConnectLocalConnection::GetUrl(const char* path) const
{
  return OmniConnectConnection::GetUrl(path);
}

bool OmniConnectLocalConnection::Initialize(const OmniConnectConnectionSettings& settings, 
  OmniConnectLogCallback logCallback, void* logUserData)
{
  bool initSuccess = OmniConnectConnection::Initialize(settings, logCallback, logUserData);
  if (initSuccess)
  {
    if (settings.WorkingDirectory.length() == 0)
    {
      OmniConnectLogMacro(OmniConnectLogLevel::ERR, "Local Output Directory not set, cannot initialize output.");
      return false;
    }

    bool workingDirExists = fs::exists(settings.WorkingDirectory);
    if (!workingDirExists)
      workingDirExists = fs::create_directory(settings.WorkingDirectory);

    if (!workingDirExists)
    {
      OmniConnectLogMacro(OmniConnectLogLevel::ERR, "Cannot create Local Output Directory, are permissions set correctly?");
      return false;
    }
    else
    {
      return true;
    }
  }
  return initSuccess;
}

void OmniConnectLocalConnection::Shutdown()
{
  OmniConnectConnection::Shutdown();
}

int OmniConnectLocalConnection::MaxSessionNr() const
{
  return OmniConnectConnection::MaxSessionNr();
}

bool OmniConnectLocalConnection::CreateFolder(const char* dirName, bool mayExist, bool combineBaseUrl) const
{
  return OmniConnectConnection::CreateFolder(dirName, mayExist, combineBaseUrl);
}

bool OmniConnectLocalConnection::RemoveFolder(const char* dirName) const
{
  return OmniConnectConnection::RemoveFolder(dirName);
}

bool OmniConnectLocalConnection::WriteFile(const char* data, size_t dataSize, const char* filePath, bool binary) const
{
  return OmniConnectConnection::WriteFile(data, dataSize, filePath, binary);
}

bool OmniConnectLocalConnection::RemoveFile(const char* filePath) const
{
  return OmniConnectConnection::RemoveFile(filePath);
}

bool OmniConnectLocalConnection::ProcessUpdates()
{
  OmniConnectConnection::ProcessUpdates();

  return true;
}

const char* OmniConnectLocalConnection::GetOmniUser(const char* serverUrl) const
{
  return OmniConnectConnection::GetOmniUser(serverUrl);
}

OmniConnectUrlInfoList OmniConnectLocalConnection::GetOmniConnectUrlInfoList(const char* serverUrl)
{
  return OmniConnectConnection::GetOmniConnectUrlInfoList(serverUrl);
}

const char* OmniConnectLocalConnection::GetOmniClientVersion()
{
  return OmniConnectConnection::GetOmniClientVersion();
}

void OmniConnectLocalConnection::SetAuthMessageBoxCallback(void* userData, OmniConnectAuthCallback callback)
{
  OmniConnectConnection::SetAuthMessageBoxCallback(userData, callback);
}

void OmniConnectLocalConnection::CancelOmniClientAuth(uint32_t authHandle)
{
  OmniConnectConnection::CancelOmniClientAuth(authHandle);
}

