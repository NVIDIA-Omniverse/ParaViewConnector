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

#ifndef OmniConnectConnection_h
#define OmniConnectConnection_h

#include "OmniConnectData.h"

#include <string>
#include <fstream>
#include <sstream>

class OmniConnectRemoteConnectionInternals;

struct OmniConnectConnectionSettings
{
  std::string HostName;
  std::string WorkingDirectory;
  bool CheckWritePermissions;
};

class OmniConnectConnection
{
public:
  OmniConnectConnection() {}
  virtual ~OmniConnectConnection() {};

  virtual const char* GetBaseUrl() const = 0;
  virtual const char* GetUrl(const char* path) const = 0;
  
  virtual bool Initialize(const OmniConnectConnectionSettings& settings, 
    OmniConnectLogCallback logCallback, void* logUserData) = 0;
  virtual void Shutdown() = 0;
  
  virtual int MaxSessionNr() const = 0;
  
  virtual bool CreateFolder(const char* dirName, bool mayExist, bool combineBaseUrl = true) const = 0;
  virtual bool RemoveFolder(const char* dirName) const = 0;
  virtual bool WriteFile(const char* data, size_t dataSize, const char* filePath, bool binary = true) const = 0;
  virtual bool RemoveFile(const char* filePath) const = 0;
  
  virtual bool ProcessUpdates() = 0;

  virtual const char* GetOmniUser(const char* serverUrl) const = 0;
  virtual OmniConnectUrlInfoList GetOmniConnectUrlInfoList(const char* serverUrl) = 0;
  virtual const char* GetOmniClientVersion() = 0;

  virtual void SetAuthMessageBoxCallback(void* userData, OmniConnectAuthCallback callback) = 0;
  virtual void CancelOmniClientAuth(uint32_t authHandle) = 0;

  static OmniConnectLogCallback LogCallback;
  static void* LogUserData;

  OmniConnectConnectionSettings Settings;

protected:

  mutable std::string TempUrl;
};


class OmniConnectLocalConnection : public OmniConnectConnection
{
public:
  OmniConnectLocalConnection();
  ~OmniConnectLocalConnection() override;

  const char* GetBaseUrl() const override;
  const char* GetUrl(const char* path) const override;

  bool Initialize(const OmniConnectConnectionSettings& settings, 
    OmniConnectLogCallback logCallback, void* logUserData) override;
  void Shutdown() override;

  int MaxSessionNr() const override;

  bool CreateFolder(const char* dirName, bool mayExist, bool combineBaseUrl = true) const override;
  bool RemoveFolder(const char* dirName) const override;
  bool WriteFile(const char* data, size_t dataSize, const char* filePath, bool binary = true) const override;
  bool RemoveFile(const char* filePath) const override;

  bool ProcessUpdates() override;

  const char* GetOmniUser(const char* serverUrl) const override;
  OmniConnectUrlInfoList GetOmniConnectUrlInfoList(const char* serverUrl) override;
  const char* GetOmniClientVersion() override;
  void SetAuthMessageBoxCallback(void* userData, OmniConnectAuthCallback callback) override;
  void CancelOmniClientAuth(uint32_t authHandle) override;

protected:
};

class OmniConnectRemoteConnection : public OmniConnectConnection
{
public:
  OmniConnectRemoteConnection();
  ~OmniConnectRemoteConnection() override;

  const char* GetBaseUrl() const override;
  const char* GetUrl(const char* path) const override;

  bool Initialize(const OmniConnectConnectionSettings& settings, 
    OmniConnectLogCallback logCallback, void* logUserData) override;
  void Shutdown() override;

  int MaxSessionNr() const override;

  bool CreateFolder(const char* dirName, bool mayExist, bool combineBaseUrl = true) const override;
  bool RemoveFolder(const char* dirName) const override;
  bool WriteFile(const char* data, size_t dataSize, const char* filePath, bool binary = true) const override;
  bool RemoveFile(const char* filePath) const override;

  bool ProcessUpdates() override;

  const char* GetOmniUser(const char* serverUrl) const override;
  OmniConnectUrlInfoList GetOmniConnectUrlInfoList(const char* serverUrl) override;
  const char* GetOmniClientVersion() override;
  void SetAuthMessageBoxCallback(void* userData, OmniConnectAuthCallback callback) override;
  void CancelOmniClientAuth(uint32_t authHandle) override;

  static void SetConnectionLogLevel(int logLevel);
  static int GetConnectionLogLevelMax();

  static bool OmniClientInitialized;
  static int NumInitializedConnInstances;

protected:

  bool CheckWritePermissions();

  OmniConnectRemoteConnectionInternals* Internals;
  bool ConnectionInitialized = false;

  static int ConnectionLogLevel;
};

#endif
