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

#include "vtkPVOmniConnectSMPipelineController.h"

#include "vtkSMSession.h"
#include "vtkSMSessionProxyManager.h"
#include "vtkSMProxyDefinitionManager.h"
#include "vtkPVProxyDefinitionIterator.h"
#include "vtkSMProxy.h"
#include "vtkObjectFactory.h"
#include "vtkCommand.h"

vtkStandardNewMacro(vtkPVOmniConnectSMPipelineController);

bool vtkPVOmniConnectSMPipelineController::InProxyRegister = false;

vtkPVOmniConnectSMPipelineController::vtkPVOmniConnectSMPipelineController() : 
  vtkSMParaViewPipelineController()
{
}

vtkPVOmniConnectSMPipelineController::~vtkPVOmniConnectSMPipelineController() 
{
}

// method to create a new proxy safely i.e. not produce warnings if definition
// is not available.
inline vtkSMProxy* vtkSafeNewProxy(vtkSMSessionProxyManager* pxm, const char* group, const char* name)
{
  if (pxm && pxm->GetPrototypeProxy(group, name))
  {
    return pxm->NewProxy(group, name);
  }
  return nullptr;
}

bool vtkPVOmniConnectSMPipelineController::RegisterOmniConnectProxyGroup(vtkSMSession* session) 
{
  bool proxiesRegistered = false;

  // Make sure it's not called recursively due to requirement to allow registration of proxies in registration callbacks from other proxies.
  if(!InProxyRegister)
  {
    InProxyRegister = true;

    // Set up the Omniverse settings proxies
    vtkSMSessionProxyManager* pxm = session->GetSessionProxyManager();
    vtkSMProxyDefinitionManager* pdm = pxm->GetProxyDefinitionManager();
    vtkPVProxyDefinitionIterator* iter = pdm->NewSingleGroupIterator(OMNI_PROXY_GROUP_NAME);
    for (iter->GoToFirstItem(); !iter->IsDoneWithTraversal(); iter->GoToNextItem())
    {
      vtkSMProxy* proxy = pxm->GetProxy(iter->GetGroupName(), iter->GetProxyName());
      // Create, register, and update proxy if it doesn't exist yet.
      if (!proxy)
      {
        proxy = vtkSafeNewProxy(pxm, iter->GetGroupName(), iter->GetProxyName());
        if (proxy)
        {
          this->InitializeProxy(proxy);
          pxm->RegisterProxy(iter->GetGroupName(), iter->GetProxyName(), proxy);
          proxy->UpdateVTKObjects();
          proxy->Delete();
          proxiesRegistered = true;
        }
      }
    }
    iter->Delete();

    InProxyRegister = false;
  }

  return proxiesRegistered;
}