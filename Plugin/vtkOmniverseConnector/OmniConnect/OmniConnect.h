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

#ifndef OmniConnect_h
#define OmniConnect_h

#include "OmniConnectData.h"
#include "OmniConnectUtilsExternal.h" // For OmniConnect_INTERFACE

class OmniConnectInternals;
class OmniConnectConnection;

class OmniConnect_INTERFACE OmniConnect
{
public:

  OmniConnect(const OmniConnectSettings& settings
    , const OmniConnectEnvironment& environment
    , OmniConnectLogCallback logCallback
  );
  ~OmniConnect();

  const OmniConnectSettings& GetSettings();
  
  //
  // Omniverse/local output setup
  //

  bool OpenConnection(bool createSession = true);
  bool GetConnectionValid() const { return ConnectionValid; }
  void CloseConnection();

  //
  // ParaView toolbar options interface for client
  //
  const char* GetUser(const char* serverUrl) const;
  OmniConnectUrlInfoList GetUrlInfoList(const char* serverUrl) const;
  bool CreateFolder(const char* url);
  int GetLatestSessionNumber() const;
  const char* GetClientVersion() const;
  void SetAuthMessageBoxCallback(void* userData, OmniConnectAuthCallback callback);
  void CancelOmniClientAuth(uint32_t authHandle);
  static bool OmniClientEnabled();

  //
  // USD interface
  //

  bool CreateActor(size_t actorId, const char* actorName);
  void DeleteActor(size_t actorId);
  
  void SetMaterialBinding(size_t actorId, size_t geomId, size_t materialId, OmniConnectGeomType geomType);
  
  size_t GetNumSceneToAnimTimes(size_t actorId);
  void RestoreSceneToAnimTimes(double* sceneToAnimTimes, size_t numSceneToAnimTimes); // Returns sceneToAnimTimes for actorId from last call to GetNumSceneToAnimTimes
  void SetSceneToAnimTime(size_t actorId, double sceneTime, const double* sceneToAnimTimes, size_t numSceneToAnimTimes); // Requires that all "animtime" timesteps of the sceneToAnimTimes tuple have already been added with UpdateMesh/Instancer.

  void FlushActorUpdates(size_t actorId);
  void FlushSceneUpdates();

  void SetActorVisibility(size_t actorId, bool visible, double animTimeStep = -1);
  void AddTimeStep(size_t actorId, double animTimeStep);
  void SetActorTransform(size_t actorId, double animTimeStep, double* transform);
  void UpdateMaterial(size_t actorId, const OmniConnectMaterialData& omniBaseMatData, double animTimeStep);
  void DeleteMaterial(size_t actorId, size_t materialId);
  void UpdateTexture(size_t actorId, size_t texId, OmniConnectSamplerData& samplerData, bool timeVarying, double animTimeStep);
  void DeleteTexture(size_t actorId, size_t texId);
  void SetGeomVisibility(size_t actorId, size_t geomId, OmniConnectGeomType geomType, bool visible, double animTimeStep = -1); //Either for all timesteps (default parameter), or at a specific timestep.
  void UpdateMesh(size_t actorId, double animTimeStep, OmniConnectMeshData& meshData, size_t materialId, 
    OmniConnectGenericArray* updatedGenericArrays, size_t numUga, OmniConnectGenericArray* deletedGenericArrays, size_t numDga);
  void UpdateInstancer(size_t actorId, double animTimeStep, OmniConnectInstancerData& instancerData, size_t materialId, 
    OmniConnectGenericArray* updatedGenericArrays, size_t numUga, OmniConnectGenericArray* deletedGenericArrays, size_t numDga);
  void UpdateCurve(size_t actorId, double animTimeStep, OmniConnectCurveData& curveData, size_t materialId,
    OmniConnectGenericArray* updatedGenericArrays, size_t numUga, OmniConnectGenericArray* deletedGenericArrays, size_t numDga);
  void UpdateVolume(size_t actorId, double animTimeStep, OmniConnectVolumeData& volumeData, size_t materialId,
    OmniConnectGenericArray* updatedGenericArrays, size_t numUga, OmniConnectGenericArray* deletedGenericArrays, size_t numDga);
  bool DeleteGeomAtTime(size_t actorId, double animTimeStep, size_t geomId, OmniConnectGeomType geomType);
  void DeleteGeom(size_t actorId, size_t geomId, OmniConnectGeomType geomType);

  //
  // Settings which can be changed at any time:
  //

  // Update contents of usd files on the omniverse side. Doesn't have an effect on file creation/deletion.
  void SetUpdateOmniContents(bool update);
  bool GetUpdateOmniContents() const { return UsdSaveEnabled; }

  // Convert generic arrays of double type to float before uploading
  void SetConvertGenericArraysDoubleToFloat(bool convert);

  // Enable/disable live behavior
  void SetLiveWorkflowEnabled(bool enable);

  // Gets set whenever an existing geom's primtype gets changed (maintained per-actor)
  bool GetAndResetGeomTypeChanged(size_t actorId);

  //
  // Static parameter interface
  //

  static void SetConnectionLogLevel(int logLevel);

protected:

  void DeleteActorFromScene(size_t actorId);

  bool ConnectionValid = false;
  OmniConnectConnection* Connection = nullptr;
  OmniConnectInternals* Internals = nullptr;

  bool UsdSaveEnabled = true;
};

#endif
