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

#ifndef vtkPVOmniConnectGlobalState_h
#define vtkPVOmniConnectGlobalState_h

#include "OmniConnectViewsModule.h"
#include "vtkObject.h"
#include "vtkSmartPointer.h"                      // needed for vtkSmartPointer
#include "vtkNew.h"
#include "vtkSMProxyManager.h"
#include "vtkPVOmniConnectNamesManager.h"

class vtkCallbackCommand;
class vtkPVOmniConnectAnimProxy;
class vtkSMProxy;

class OMNICONNECTVIEWS_EXPORT vtkPVOmniConnectGlobalState : public vtkObject
{
public:
  static vtkPVOmniConnectGlobalState* New();
  vtkTypeMacro(vtkPVOmniConnectGlobalState, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent);

  /**
   * Access the singleton.
   */
  static vtkPVOmniConnectGlobalState* GetInstance();

  // Management of proxies and qt plugin side
  static void OnRegisterEvent(vtkObject*, unsigned long, void*, void*);
  static void OnConnectionEvent(vtkObject*, unsigned long, void*, void*);
  static void RegisterOmniConnectProxies(vtkPVOmniConnectGlobalState* globalState);
  static void RegisterNameProxyPair(vtkSMProxyManager::RegisteredProxyInformation* registerInfo, bool isRegisterEvent);
  static void ResetNameProxyPairs();
  static void InitializeSpxmObserver(vtkPVOmniConnectGlobalState* globalState);

  // Pushing animation objects to server side via proxy
  void ObserveAnimationScene();
  void UpdateAnimatedObjectIds(vtkObject* caller, unsigned long eventID, void*);

  void LoadQtPlugin();

  // Explicitly set the last created vtkPVOmniConnectAnimProxy as one from the registered vtkSMProxy
  void SetRegisteredAnimProxy(vtkPVOmniConnectAnimProxy* proxy) { this->OmniConnectAnimProxy = proxy; }
  void ResetRegisteredAnimProxy() { this->OmniConnectAnimProxy = nullptr; }
  // Return the vtkPVOmniConnectAnimProxy from the registered vtkSMProxy
  vtkPVOmniConnectAnimProxy* GetOmniConnectAnimProxy() const { return this->OmniConnectAnimProxy; }

  vtkPVOmniConnectNamesManager& GetNamesManager() { return NamesManager; }

protected:
  vtkPVOmniConnectGlobalState();
  ~vtkPVOmniConnectGlobalState();

  vtkPVOmniConnectAnimProxy* OmniConnectAnimProxy = nullptr;
  vtkSMProxy* AnimationSceneProxy = nullptr;

  vtkNew<vtkCallbackCommand> RegisterObserver;
  vtkNew<vtkCallbackCommand> ConnectionObserver;
  unsigned long RegisterObserverTag = 0;
  unsigned long UnRegisterObserverTag = 0;
  unsigned long ConnectionObserverTag = 0;

  static vtkSmartPointer<vtkPVOmniConnectGlobalState> Instance;
  static bool QtInitialized;

  vtkPVOmniConnectNamesManager NamesManager;

private:
  vtkPVOmniConnectGlobalState(const vtkPVOmniConnectGlobalState&) = delete;
  void operator=(const vtkPVOmniConnectGlobalState&) = delete;
};

#endif
