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

#ifndef vtkPVOmniConnectAnimProxy_h
#define vtkPVOmniConnectAnimProxy_h

#include "OmniConnectViewsModule.h"
#include "vtkNew.h" // for vtkNew
#include "vtkObject.h"
#include "vtkSmartPointer.h"

#include <vector>
#include <string>
#include <utility>

class vtkSMProxy;
class vtkExecutive;

class OMNICONNECTVIEWS_EXPORT vtkPVOmniConnectAnimProxy : public vtkObject
{
public:
  using ExecutiveListType = std::vector<std::pair<std::pair<bool,bool>,vtkSmartPointer<vtkExecutive>>>; // The bool pair are temporaries for 'visited downstream' and 'actorinputname assigned'

  static vtkPVOmniConnectAnimProxy* New();
  vtkTypeMacro(vtkPVOmniConnectAnimProxy, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Helper function to get the vtkObjects from the respective ids
  void GetAnimatedObjects(std::vector<vtkObjectBase*>& animatedObjects);

  // To get the registered names from respective ids
  const char* GetRegisteredObjectNameFromAlgorithm(vtkObjectBase* alg);

  // Automatically update actor info from all registered object names
  void UpdateActorInfoFromRegisteredObjectNames();

  // Convenience function for making the proxy globally available on the server
  void SetRegisterGlobally(unsigned int value);

  // Animated object id proxy set/get methods.

  void SetNumberOfAnimatedObjectIds(int size);
  int GetNumberOfAnimatedObjectIds() const;
  void SetAnimatedObjectId(int index, unsigned int value);
  unsigned int GetAnimatedObjectId(int index) const;

  // Registered object proxy set/get methods
  void SetNumberOfRegisteredObjectIds(int size);
  int GetNumberOfRegisteredObjectIds() const;
  void SetRegisteredObjectId(int index, unsigned int value);
  unsigned int GetRegisteredObjectId(int index) const;

  void SetNumberOfRegisteredObjectNames(int size);
  int GetNumberOfRegisteredObjectNames() const;
  void SetRegisteredObjectName(int index, const char* value);
  const char* GetRegisteredObjectName(int index) const;

  static const char* OMNICONNECT_GROUP_NAME;
  static const char* OMNICONNECT_PROXY_NAME;

protected:
  vtkPVOmniConnectAnimProxy();
  ~vtkPVOmniConnectAnimProxy();

  std::vector<unsigned int> AnimatedObjectIds;

  // The animproxy also carries information about which proxies have been registered in general, with their actual (UI) names.
  // So consider renaming the proxy class altogether.
  std::vector<unsigned int> RegisteredObjectIds;
  std::vector<std::string> RegisteredObjectNames;

  // Hack to keep consumers from being deleted while still being referenced downstream from a registered object
  // Fixes dangling pointers when a renderview is created, destroyed and created again.
  // Doubles as a cache for determining which actorinputnames to delete.
  ExecutiveListType DownstreamConsumersFix;

private:
  vtkPVOmniConnectAnimProxy(const vtkPVOmniConnectAnimProxy&); // Not implemented
  void operator=(const vtkPVOmniConnectAnimProxy&); // Not implemented
};

#endif
