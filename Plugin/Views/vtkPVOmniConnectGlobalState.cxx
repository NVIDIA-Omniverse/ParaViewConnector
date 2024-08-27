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

#include "vtkPVOmniConnectGlobalState.h"

#include "vtkOmniConnectLogCallback.h"
#include "vtkPVOmniConnectSMPipelineController.h"
#include "vtkPVOmniConnectAnimProxy.h"

#include "vtkObjectFactory.h"
#include "vtkCallbackCommand.h"
#include "vtkPVSession.h"
#include "vtkSMSession.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkProcessModule.h"
#include "vtkSMPluginManager.h"
#include "vtkSMProxy.h"
#include "vtkSMIntVectorProperty.h"
#include "vtkSMStringVectorProperty.h"
#include "vtkSMPropertyHelper.h"
#include "vtkPVKeyFrameAnimationCueForProxies.h"
#include "vtkSMSourceProxy.h"
#include "vtkPVRepresentationAnimationHelper.h"
#include "vtkDynamicLoader.h"

#include <cassert>
#include <cstring>

//----------------------------------------------------------------------------
vtkSmartPointer<vtkPVOmniConnectGlobalState> vtkPVOmniConnectGlobalState::Instance;
bool vtkPVOmniConnectGlobalState::QtInitialized = false;

//----------------------------------------------------------------------------
vtkPVOmniConnectGlobalState* vtkPVOmniConnectGlobalState::New()
{
  vtkPVOmniConnectGlobalState* instance = vtkPVOmniConnectGlobalState::GetInstance();
  assert(instance);
  instance->Register(NULL); // Just increases refcount signifying that client now owns a reference to the singleton as if it were a regular object (including ref decrease once done)
  return instance;
}

//----------------------------------------------------------------------------
vtkPVOmniConnectGlobalState* vtkPVOmniConnectGlobalState::GetInstance()
{
  if (!vtkPVOmniConnectGlobalState::Instance)
  {
    // Create and initialize the settings instance
    vtkPVOmniConnectGlobalState* instance = new vtkPVOmniConnectGlobalState();
    instance->InitializeObjectBase();
    vtkPVOmniConnectGlobalState::Instance.TakeReference(instance); // Keep the refcount at 1 and transfer to smartpointer
  }
  return vtkPVOmniConnectGlobalState::Instance;
}

//----------------------------------------------------------------------------
vtkPVOmniConnectGlobalState::vtkPVOmniConnectGlobalState()
{
  vtkPVOmniConnectGlobalState::InitializeSpxmObserver(this);

  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  if(pm)
  {
    // Event that gets called at server connection
    this->ConnectionObserver->SetCallback(&vtkPVOmniConnectGlobalState::OnConnectionEvent);
    this->ConnectionObserver->SetClientData(this);
    this->ConnectionObserverTag = pm->AddObserver(vtkCommand::ConnectionCreatedEvent, this->ConnectionObserver.Get());
  }
}

//----------------------------------------------------------------------------
vtkPVOmniConnectGlobalState::~vtkPVOmniConnectGlobalState()
{
  if (vtkSMProxyManager::IsInitialized())
  {
    vtkSMSessionProxyManager* spxm = vtkSMProxyManager::GetProxyManager()->GetActiveSessionProxyManager();
    if (spxm && this->RegisterObserverTag)
      spxm->RemoveObserver(this->RegisterObserverTag);
    if (spxm && this->UnRegisterObserverTag)
      spxm->RemoveObserver(this->UnRegisterObserverTag);
  }

  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  if(pm && this->ConnectionObserverTag)
  {
    pm->RemoveObserver(this->ConnectionObserverTag);
  }
}

void vtkPVOmniConnectGlobalState::InitializeSpxmObserver(vtkPVOmniConnectGlobalState* globalState)
{
  // This has to be initialized at every new connection, to keep observing the session proxy manager's registerevents
  if (vtkSMProxyManager::IsInitialized())
  {
    vtkSMSessionProxyManager* spxm = vtkSMProxyManager::GetProxyManager()->GetActiveSessionProxyManager();
    if (spxm)
    {
      // Event that gets called after completion of settings constructor
      globalState->RegisterObserver->SetCallback(&vtkPVOmniConnectGlobalState::OnRegisterEvent);
      globalState->RegisterObserver->SetClientData(globalState);
      globalState->RegisterObserverTag = spxm->AddObserver(vtkCommand::RegisterEvent, globalState->RegisterObserver.Get());
      globalState->UnRegisterObserverTag = spxm->AddObserver(vtkCommand::UnRegisterEvent, globalState->RegisterObserver.Get());
    }
  }
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectGlobalState::RegisterOmniConnectProxies(vtkPVOmniConnectGlobalState* globalState)
{
  // Force creation of all relevant proxies. Any earlier point would trigger double creation of the settings class;
  // creation of the settings instance (ie. its singleton constructor) has to be fully finished before registering the proxy.
  vtkSMSession* activeSession = vtkSMProxyManager::GetProxyManager()->GetActiveSession();
  if(activeSession)
  {
    vtkNew<vtkPVOmniConnectSMPipelineController> controller;
    bool proxiesRegistered = controller->RegisterOmniConnectProxyGroup(activeSession);

    if(proxiesRegistered)
    {
      // Let the anim proxy know that it can set itself to the global state (for convenient access to the proxy on the server)
      vtkSMProxy* omniConnectAnimProxy = vtkSMProxyManager::GetProxyManager()->GetProxy(vtkPVOmniConnectAnimProxy::OMNICONNECT_GROUP_NAME, vtkPVOmniConnectAnimProxy::OMNICONNECT_PROXY_NAME);
      assert(omniConnectAnimProxy);

      // Reset the anim proxy, in case the process role changes to pure client (in which case the RegisterGlobally will not reach the client global state)
      globalState->ResetRegisteredAnimProxy();

      // Allow the anim proxy to register globally
      vtkSMProperty* regProp = omniConnectAnimProxy->GetProperty("RegisterGlobally");
      assert(regProp);
      vtkSMIntVectorProperty* regVectorProp = vtkSMIntVectorProperty::SafeDownCast(regProp);
      assert(regVectorProp != nullptr);
      regVectorProp->SetElement(0,1);

      omniConnectAnimProxy->UpdateVTKObjects();
    }
  }

  if (!vtkPVOmniConnectSMPipelineController::InProxyRegister)
  {
    // Observe addition and removal of animation cues
    vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
    vtkPVSession* session = vtkPVSession::SafeDownCast(pm->GetSession());
    if(session && session->HasProcessRole(vtkPVSession::CLIENT))
      globalState->ObserveAnimationScene();
  }
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectGlobalState::OnRegisterEvent(vtkObject*, unsigned long eid, void* clientData, void* callData)
{
  vtkSMProxyManager::RegisteredProxyInformation* registerInfo = reinterpret_cast<vtkSMProxyManager::RegisteredProxyInformation*>(callData);
  bool isRegisterEvent = (eid == vtkCommand::RegisterEvent); // Otherwise an unregisterevent

  if(!strcmp(registerInfo->GroupName, "settings") && !strcmp(registerInfo->ProxyName, "OmniConnectSettingsPlaceholder"))
    vtkPVOmniConnectGlobalState::RegisterOmniConnectProxies((vtkPVOmniConnectGlobalState*)clientData);

  // Trigger initialization of the Qt part of the plugin. Make sure this is done after proxy registration.
  if (!vtkPVOmniConnectSMPipelineController::InProxyRegister)
  {
    if(isRegisterEvent && !vtkPVOmniConnectGlobalState::QtInitialized)
    {
      vtkPVOmniConnectGlobalState::QtInitialized = true;
      ((vtkPVOmniConnectGlobalState*)clientData)->LoadQtPlugin();
    }

    vtkPVOmniConnectGlobalState::RegisterNameProxyPair(registerInfo, isRegisterEvent);
  }
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectGlobalState::OnConnectionEvent(vtkObject*, unsigned long, void* clientData, void*)
{
  vtkPVOmniConnectGlobalState::InitializeSpxmObserver(vtkPVOmniConnectGlobalState::GetInstance());
  vtkPVOmniConnectGlobalState::RegisterOmniConnectProxies((vtkPVOmniConnectGlobalState*)clientData);
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectGlobalState::RegisterNameProxyPair(vtkSMProxyManager::RegisteredProxyInformation* registerInfo, bool isRegisterEvent)
{
  vtkSMProxy* omniConnectAnimProxy = vtkSMProxyManager::GetProxyManager()->GetProxy(vtkPVOmniConnectAnimProxy::OMNICONNECT_GROUP_NAME, vtkPVOmniConnectAnimProxy::OMNICONNECT_PROXY_NAME);

  // Only actual pipeline object names are recorded
  if(!omniConnectAnimProxy ||
    (strcmp(registerInfo->GroupName, "sources") && strcmp(registerInfo->GroupName, "filters") && strcmp(registerInfo->GroupName, "extractors")) )
    return;

  vtkSMProperty* nameProp = omniConnectAnimProxy->GetProperty("RegisteredObjectNames");
  assert(nameProp);
  vtkSMStringVectorProperty* nameVectorProp = vtkSMStringVectorProperty::SafeDownCast(nameProp);
  assert(nameVectorProp != nullptr);
  unsigned int numNames = nameVectorProp->GetNumberOfElements();

  vtkSMProperty* idProp = omniConnectAnimProxy->GetProperty("RegisteredObjectIds");
  assert(idProp);
  vtkSMIntVectorProperty* idVectorProp = vtkSMIntVectorProperty::SafeDownCast(idProp);
  assert(idVectorProp != nullptr);
  unsigned int numIds = idVectorProp->GetNumberOfElements();

  vtkTypeUInt32 proxyID = registerInfo->Proxy->GetGlobalID();

  assert(numNames == numIds);
  bool found = false;
  for(unsigned int i = 0; i < numIds; ++i)
  {
    if(idVectorProp->GetElement(i) == proxyID)
    {
      if(isRegisterEvent)
      {
        // Overwrite the name of the found id
        nameVectorProp->SetElement(i, registerInfo->ProxyName);
        found = true;
      }
      else if(std::strcmp(registerInfo->GroupName, nameVectorProp->GetElement(i)) == 0) // Only unregister if name still matches
      {
        unsigned int backIdx = numIds-1;
        if(i < backIdx)
        {
          // Replace found name and id with back of the list
          idVectorProp->SetElement(i, idVectorProp->GetElement(backIdx));
          nameVectorProp->SetElement(i, nameVectorProp->GetElement(backIdx));
        }
        // Shorten the name and id list by 1
        nameVectorProp->SetNumberOfElements(backIdx);
        idVectorProp->SetNumberOfElements(backIdx);
        found = true;
      }
    }
  }
  if(isRegisterEvent && !found)
  {
    // Add the name and id to the back of the list
    nameVectorProp->SetNumberOfElements(numNames+1);
    nameVectorProp->SetElement(numNames, registerInfo->ProxyName);
    idVectorProp->SetNumberOfElements(numIds+1);
    idVectorProp->SetElement(numIds, proxyID);
  }

  omniConnectAnimProxy->UpdateVTKObjects();
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectGlobalState::ResetNameProxyPairs()
{
  vtkSMProxy* omniConnectAnimProxy = vtkSMProxyManager::GetProxyManager()->GetProxy(vtkPVOmniConnectAnimProxy::OMNICONNECT_GROUP_NAME, vtkPVOmniConnectAnimProxy::OMNICONNECT_PROXY_NAME);
  if(!omniConnectAnimProxy)
    return;

  vtkSMProperty* nameProp = omniConnectAnimProxy->GetProperty("RegisteredObjectNames");
  assert(nameProp);
  vtkSMStringVectorProperty* nameVectorProp = vtkSMStringVectorProperty::SafeDownCast(nameProp);
  assert(nameVectorProp != nullptr);
  vtkSMProperty* idProp = omniConnectAnimProxy->GetProperty("RegisteredObjectIds");
  assert(idProp);
  vtkSMIntVectorProperty* idVectorProp = vtkSMIntVectorProperty::SafeDownCast(idProp);
  assert(idVectorProp != nullptr);

  nameVectorProp->SetNumberOfElements(0);
  idVectorProp->SetNumberOfElements(0);

  omniConnectAnimProxy->UpdateVTKObjects();
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectGlobalState::LoadQtPlugin()
{
  // Detect client and load Qt plugin if available (first entry point of Omniverse Connector)
  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  vtkPVSession* session = vtkPVSession::SafeDownCast(pm->GetSession());
  vtkSMPluginManager* pluginManager = vtkSMProxyManager::GetProxyManager()->GetPluginManager();

  if (session && session->HasProcessRole(vtkPVSession::CLIENT) && pluginManager)
  {
    const std::string& programPath = pm->GetSelfDir();
    std::string qtLibPath = programPath +
#ifdef _WIN32
      "/Qt5Core.dll";
#else
      "/../lib/libQt5Core.so";
#endif

    vtkLibHandle lib = vtkDynamicLoader::OpenLibrary(qtLibPath.c_str());

    if (lib)
    {
      std::string pluginLibName = 
#ifdef _WIN32
        "OmniverseConnectorQt.dll";
#else
        "OmniverseConnectorQt.so";
#endif

      std::string pluginLibPathRel = std::string("./") + PARAVIEW_SUBDIR + "/OmniverseConnector/OmniverseConnectorQt/" + pluginLibName;

      std::string pluginLibPathAbs = programPath +
#ifdef _WIN32
        "/"
#else
        "/../lib/"
#endif
        + pluginLibPathRel;

      bool pluginLoaded = pluginManager->LoadLocalPlugin(pluginLibPathAbs.c_str());
      // Try just the library via any user-supplied library paths, in case pv runs from a read-only location.
      if(!pluginLoaded)
        pluginLoaded = pluginManager->LoadLocalPlugin(pluginLibName.c_str());

      if (pluginLoaded)
        vtkOmniConnectLogCallback::Callback(OmniConnectLogLevel::STATUS, nullptr, "Omniverse Connector Qt plugin loaded");
      else
        vtkOmniConnectLogCallback::Callback(OmniConnectLogLevel::WARNING, nullptr, "Omniverse Connector Qt plugin failed to load");
    }
    else
      vtkOmniConnectLogCallback::Callback(OmniConnectLogLevel::WARNING, nullptr, "Qt5Core library not found, skipping Qt support for Omniverse Connector");
  }
}

void vtkPVOmniConnectGlobalState::ObserveAnimationScene()
{
  vtkSMSession* smSession = vtkSMProxyManager::GetProxyManager()->GetActiveSession();
  vtkSMSessionProxyManager* pxm = smSession ? smSession->GetSessionProxyManager() : nullptr;
  vtkSMProxy* animationScene = pxm ? pxm->FindProxy("animation", "animation", "AnimationScene") : nullptr;
  if(!animationScene) 
    return;

  // Only register to the animation scene if it's new
  if(AnimationSceneProxy == animationScene)
    return;
  else
    AnimationSceneProxy = animationScene;

  vtkSMProxy* omniConnectAnimProxy = vtkSMProxyManager::GetProxyManager()->GetProxy(vtkPVOmniConnectAnimProxy::OMNICONNECT_GROUP_NAME, vtkPVOmniConnectAnimProxy::OMNICONNECT_PROXY_NAME);
  if(!omniConnectAnimProxy)
    return;

  vtkSMProperty* cueProp = animationScene->GetProperty("Cues");
  if(cueProp)
  {
    // Clear the animated objects
    vtkSMProperty* animProp = omniConnectAnimProxy->GetProperty("AnimatedObjectIds");
    assert(animProp);
    vtkSMIntVectorProperty* vectorProp = vtkSMIntVectorProperty::SafeDownCast(animProp);
    assert(vectorProp != nullptr);
    vectorProp->SetNumberOfElements(0);

    // Add observer to new proxy to record changes
    cueProp->AddObserver(vtkCommand::ModifiedEvent, this, &vtkPVOmniConnectGlobalState::UpdateAnimatedObjectIds);

    omniConnectAnimProxy->UpdateVTKObjects();
  }
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectGlobalState::UpdateAnimatedObjectIds(vtkObject* caller, unsigned long eventID, void*)
{
  vtkSMProxy* omniConnectAnimProxy = vtkSMProxyManager::GetProxyManager()->GetProxy(vtkPVOmniConnectAnimProxy::OMNICONNECT_GROUP_NAME, vtkPVOmniConnectAnimProxy::OMNICONNECT_PROXY_NAME);
  vtkSMProperty* cueProp = vtkSMProperty::SafeDownCast(caller);
  if(!omniConnectAnimProxy || !cueProp)
    return;

  vtkSMProperty* animProp = omniConnectAnimProxy->GetProperty("AnimatedObjectIds");
  assert(animProp);
  vtkSMIntVectorProperty* vectorProp = vtkSMIntVectorProperty::SafeDownCast(animProp);
  assert(vectorProp != nullptr);
  
  int numAnimatedProxies = 0;
  
  vtkSMPropertyHelper cueHelper(cueProp);
  for (unsigned int cc = 0, max = cueHelper.GetNumberOfElements(); cc < max; cc++)
  {
    // Extract the client-side cue of proxies, from which we extract their GlobalIds
    vtkSMProxy* cue = cueHelper.GetAsProxy(cc);
    vtkObjectBase* clientObj = cue->GetClientSideObject();
    vtkPVKeyFrameAnimationCueForProxies* clientCue = nullptr;
    if (clientObj && clientObj->IsA("vtkPVKeyFrameAnimationCueForProxies")) 
      clientCue = static_cast<vtkPVKeyFrameAnimationCueForProxies*>(clientObj);

    if(clientCue)
    {
      vtkSMProxy* smProxy = clientCue->GetAnimatedProxy();
      vtkSMSourceProxy* animatedProxy = vtkSMSourceProxy::SafeDownCast(smProxy);
      // Sometimes the actual animated proxy's source is a property of the cue's proxy
      if(smProxy && !animatedProxy)
      {
        vtkSMProperty* srcProp = smProxy->GetProperty("Source");
        if(srcProp)
        {
          vtkSMPropertyHelper animHelper(srcProp);
          for(unsigned int sourceIdx = 0, maxSrc = animHelper.GetNumberOfElements(); sourceIdx < maxSrc; ++sourceIdx)
          {
            smProxy = animHelper.GetAsProxy(sourceIdx);
            animatedProxy = vtkSMSourceProxy::SafeDownCast(smProxy);

            int idx = numAnimatedProxies++;
            vectorProp->SetNumberOfElements(numAnimatedProxies);
            vectorProp->SetElement(idx, animatedProxy->GetGlobalID());
          }
        }
      }
      else
      {
        // Add the animated client-side object to the list
        int idx = numAnimatedProxies++;
        vectorProp->SetNumberOfElements(numAnimatedProxies);
        vectorProp->SetElement(idx, animatedProxy->GetGlobalID());
      }
    }
  }

  if(!numAnimatedProxies)
    vectorProp->SetNumberOfElements(0);

  omniConnectAnimProxy->UpdateVTKObjects();
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectGlobalState::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
