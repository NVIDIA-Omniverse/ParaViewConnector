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

#include "vtkPVOmniConnectAnimProxy.h"

#include "vtkPVOmniConnectGlobalState.h"
#include "vtkOmniConnectLogCallback.h"
#include "vtkPVOmniConnectNamesManager.h"

#include "vtkObjectFactory.h"
#include "vtkPVSessionBase.h"
#include "vtkProcessModule.h"
#include "vtkPVSessionCore.h"
#include "vtkSIProxy.h"
#include "vtkAlgorithm.h"
#include "vtkExecutive.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkPVDataRepresentation.h"
#include "vtkInformationExecutivePortVectorKey.h"

#include <cassert>

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkPVOmniConnectAnimProxy);

//----------------------------------------------------------------------------
const char* vtkPVOmniConnectAnimProxy::OMNICONNECT_GROUP_NAME = "OmniConnectProxyGroup";
const char* vtkPVOmniConnectAnimProxy::OMNICONNECT_PROXY_NAME = "OmniConnectAnimProxy";

namespace
{
  vtkPVSessionCore* GetSessionCore()
  {
    vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
    vtkPVSessionBase* session = pm ? vtkPVSessionBase::SafeDownCast(pm->GetSession()) : nullptr;
    vtkPVSessionCore* sessionCore = session ? session->GetSessionCore() : nullptr;
    return sessionCore;
  }

  void ClearActorInputNames(const vtkPVOmniConnectAnimProxy::ExecutiveListType& executives, vtkPVSessionCore* sessionCore)
  {
    vtkPVOmniConnectNamesManager& namesManager = vtkPVOmniConnectGlobalState::GetInstance()->GetNamesManager();
    for(auto& boolExecPair : executives)
    {
      if(!boolExecPair.first.second) // If the actorinputname hasn't been set in the last call to SetDownstreamActorNames
      {
        vtkPVDataRepresentation* dataRepr = vtkPVDataRepresentation::SafeDownCast(boolExecPair.second->GetAlgorithm());
        if(dataRepr)
        {
          vtkProp3D* actor = vtkPVOmniConnectNamesManager::GetActorFromRepresentation(dataRepr);
          if(actor)
          {
            vtkInformation* actInfo = vtkPVOmniConnectNamesManager::GetOrCreatePropertyKeys(actor);
            namesManager.ClearActorInputName(actInfo, sessionCore);
          }
        }
      }
    }
  }

  void SetDownstreamActorNames(vtkExecutive* exec, const char* name, vtkPVSessionCore* sessionCore,
    vtkPVOmniConnectAnimProxy::ExecutiveListType& executiveList, int recursion, int inputPort = -1, int inputConn = -1)
  {
    bool isInputActorName = inputPort > 0;

    auto exIt = executiveList.begin();
    for(; exIt != executiveList.end() && exIt->second.GetPointer() != exec; ++exIt)
    {}

    if(exIt == executiveList.end())
      executiveList.push_back({{true, isInputActorName}, exec});
    else
      exIt->first = {true, exIt->first.second || isInputActorName};

    vtkPVDataRepresentation* dataRepr = vtkPVDataRepresentation::SafeDownCast(exec->GetAlgorithm());
    if(dataRepr)
    {
      // A bit of a hack to make sure we don't set actornames on representations that are too deep down, which can happen in case of slice representations.
      // In that case, there is no dataRepr in the chain between the slice and the resampletoimage actor, and latter gets the name of the former.
      // So if we are dealing with a normal actor name and the recursion has gone beyond a certain depth, don't set the actor name.
      // Input actor names can go as deep as necessary.
      // Either way, if a branch finds a datarepresentation, don't traverse further (inputnames are always set via another branch)
      if(isInputActorName || recursion <= 2)
      {
        vtkProp3D* actor = vtkPVOmniConnectNamesManager::GetActorFromRepresentation(dataRepr);
        if(actor)
        {
          vtkInformation* actInfo = vtkPVOmniConnectNamesManager::GetOrCreatePropertyKeys(actor);

          vtkPVOmniConnectNamesManager& namesManager = vtkPVOmniConnectGlobalState::GetInstance()->GetNamesManager();
          namesManager.SetActorName(dataRepr->GetView(), dataRepr, actInfo, name, sessionCore, isInputActorName);
        }
      }
    }
    else
    {
      // Recursively search for datarepresentations, and set the names on their actors
      int numPorts = exec->GetNumberOfOutputPorts();
      for (int port = 0; port < numPorts; ++port)
      {
        vtkInformationVector* outputInfo = exec->GetOutputInformation();
        vtkInformation* info = outputInfo->GetInformationObject(port);

        vtkExecutive** consumers = vtkExecutive::CONSUMERS()->GetExecutives(info);
        int consumerCount = vtkExecutive::CONSUMERS()->Length(info);
        for(int consumerIdx = 0; consumerIdx < consumerCount; ++consumerIdx)
        {
          vtkExecutive* consumer = consumers[consumerIdx];

          // Find the input port/connection on the consumer matching the producer, to allow for recognition of auxiliary (>0) port usage
          // to determine which name key to set on the actor.
          int matchingInputPort = -1;
          int matchingInputConn = -1;
          for(int cInputPortIdx = 0; cInputPortIdx < consumer->GetNumberOfInputPorts(); ++cInputPortIdx)
          {
            for(int cInputConnIdx = 0; cInputConnIdx < consumer->GetNumberOfInputConnections(cInputPortIdx); ++cInputConnIdx)
            {
              vtkExecutive* consumerParent = consumer->GetInputExecutive(cInputPortIdx, cInputConnIdx);
              if(consumerParent == exec)
              {
                matchingInputPort = cInputPortIdx;
                matchingInputConn = cInputConnIdx;
              }
            }
          }
          SetDownstreamActorNames(consumer, name, sessionCore, executiveList, recursion+1, matchingInputPort, matchingInputConn);
        }
      }
    }
  }
}

//----------------------------------------------------------------------------
vtkPVOmniConnectAnimProxy::vtkPVOmniConnectAnimProxy()
{
}

//----------------------------------------------------------------------------
vtkPVOmniConnectAnimProxy::~vtkPVOmniConnectAnimProxy()
{
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectAnimProxy::GetAnimatedObjects(std::vector<vtkObjectBase*>& animatedObjects)
{
  vtkPVSessionCore* sessionCore = GetSessionCore();
  if(!sessionCore)
    return;

  size_t numElts = this->AnimatedObjectIds.size();

  animatedObjects.resize(numElts);
  for(size_t i = 0; i < numElts; ++i)
  {
    int objectId = AnimatedObjectIds[i];
    vtkSIProxy* siProxy = vtkSIProxy::SafeDownCast(sessionCore->GetSIObject(objectId));
    if(siProxy)
    {
      vtkObjectBase* vtkObject = siProxy->GetVTKObject();
      animatedObjects[i] = vtkObject;
    }
  }
}

//----------------------------------------------------------------------------
const char* vtkPVOmniConnectAnimProxy::GetRegisteredObjectNameFromAlgorithm(vtkObjectBase* alg)
{
  vtkPVSessionCore* sessionCore = GetSessionCore();
  if(!sessionCore)
    return nullptr;

  assert(RegisteredObjectIds.size() == RegisteredObjectNames.size());
  for(size_t i = 0; i < RegisteredObjectIds.size(); ++i)
  {
    vtkSIProxy* siProxy = vtkSIProxy::SafeDownCast(sessionCore->GetSIObject(RegisteredObjectIds[i]));
    if(siProxy)
    {
      if(siProxy->GetVTKObject() == alg)
        return RegisteredObjectNames[i].c_str();
    }
  }

  return nullptr;
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectAnimProxy::UpdateActorInfoFromRegisteredObjectNames()
{
  vtkPVSessionCore* sessionCore = GetSessionCore();
  if(!sessionCore)
    return;

  for(auto& elt : DownstreamConsumersFix)
    elt.first = {false, false};

  assert(RegisteredObjectIds.size() == RegisteredObjectNames.size());
  for(size_t i = 0; i < RegisteredObjectIds.size(); ++i)
  {
    vtkSIProxy* siProxy = vtkSIProxy::SafeDownCast(sessionCore->GetSIObject(RegisteredObjectIds[i]));
    if(siProxy)
    {
      vtkObjectBase* obj = siProxy->GetVTKObject();
      if(vtkAlgorithm* alg = vtkAlgorithm::SafeDownCast(obj))
      {
        SetDownstreamActorNames(alg->GetExecutive(), RegisteredObjectNames[i].c_str(), sessionCore, DownstreamConsumersFix, 0);
      }
    }
  }

  // Input names have to be explicitly reset each time, as downstream connections may disappear.
  ClearActorInputNames(DownstreamConsumersFix, sessionCore);

  auto it = DownstreamConsumersFix.begin();
  while(it != DownstreamConsumersFix.end())
  {
    if(it->first.first)
      ++it;
    else
      it = DownstreamConsumersFix.erase(it);
  }
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectAnimProxy::SetRegisterGlobally(unsigned int value)
{
  // Abuse the global singleton to broadcast the vtkPVOmniConnectAnimProxy instance.
  // Useful on the server side to retrieve properties from this class.
  vtkPVOmniConnectGlobalState::GetInstance()->SetRegisteredAnimProxy(this);
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectAnimProxy::SetNumberOfAnimatedObjectIds(int size)
{
  AnimatedObjectIds.resize(size);
  this->Modified();
}

//----------------------------------------------------------------------------
int vtkPVOmniConnectAnimProxy::GetNumberOfAnimatedObjectIds() const
{
  return (int)AnimatedObjectIds.size();
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectAnimProxy::SetAnimatedObjectId(int index, unsigned int value)
{
  if(index < AnimatedObjectIds.size())
  {
    AnimatedObjectIds[index] = value;
    this->Modified();
  }
}

//----------------------------------------------------------------------------
unsigned int vtkPVOmniConnectAnimProxy::GetAnimatedObjectId(int index) const
{
  if(index < AnimatedObjectIds.size())
  {
    return AnimatedObjectIds[index];
  }
  return 0;
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectAnimProxy::SetNumberOfRegisteredObjectIds(int size)
{
  RegisteredObjectIds.resize(size);
  this->Modified();
}

//----------------------------------------------------------------------------
int vtkPVOmniConnectAnimProxy::GetNumberOfRegisteredObjectIds() const
{
  return (int)RegisteredObjectIds.size();
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectAnimProxy::SetRegisteredObjectId(int index, unsigned int value)
{
  if(index < RegisteredObjectIds.size())
  {
    RegisteredObjectIds[index] = value;
    this->Modified();
  }
}

//----------------------------------------------------------------------------
unsigned int vtkPVOmniConnectAnimProxy::GetRegisteredObjectId(int index) const
{
  if(index < RegisteredObjectIds.size())
  {
    return RegisteredObjectIds[index];
  }
  return 0;
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectAnimProxy::SetNumberOfRegisteredObjectNames(int size)
{
  RegisteredObjectNames.resize(size);
  this->Modified();
}

//----------------------------------------------------------------------------
int vtkPVOmniConnectAnimProxy::GetNumberOfRegisteredObjectNames() const
{
  return (int)RegisteredObjectNames.size();
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectAnimProxy::SetRegisteredObjectName(int index, const char* value)
{
  if(index < RegisteredObjectNames.size())
  {
    RegisteredObjectNames[index] = value;
    this->Modified();
  }
}

//----------------------------------------------------------------------------
const char* vtkPVOmniConnectAnimProxy::GetRegisteredObjectName(int index) const
{
  if(index < RegisteredObjectNames.size())
  {
    return RegisteredObjectNames[index].c_str();
  }
  return nullptr;
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectAnimProxy::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}



