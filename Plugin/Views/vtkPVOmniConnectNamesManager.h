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

#ifndef vtkPVOmniConnectNamesManager_h
#define vtkPVOmniConnectNamesManager_h

#include "OmniConnectViewsModule.h"

#include <map>
#include <set>
#include <string>

class vtkDataRepresentation;
class vtkProp3D;
class vtkInformation;
class vtkPVDataRepresentation;
class vtkPVSessionCore;
class vtkView;

class vtkPVOmniConnectNamesManager
{
  public:
    vtkPVOmniConnectNamesManager();
    ~vtkPVOmniConnectNamesManager();

    // Given a datarepresentation and an actor info, set a name which belongs to the particular view
    void ClearActorInputName(vtkInformation* actInfo, vtkPVSessionCore* sessionCore);
    void SetActorName(vtkView* view, vtkPVDataRepresentation* dataRepr, vtkInformation* actInfo, const char* suggestedName,
      vtkPVSessionCore* sessionCore, bool isInputActorName);

    // Convenience functions to get to info objects
    static vtkProp3D* GetActorFromRepresentation(vtkDataRepresentation* dataRepr);
    static vtkInformation* GetOrCreatePropertyKeys(vtkProp3D* actor);

  protected:
    using NameSet = std::set<std::string>;
    using ViewNameMap = std::map<vtkView*, NameSet>;

    const char* GetUniqueName(vtkView* view, std::string& baseName);
    const char* GetSurfaceName(vtkView* view, std::string& baseName);
    bool NameIsUnique(std::string& name, const NameSet& uniqueViewNames) const;

    // Every view has its own set of unique names (so not unique between views)
    ViewNameMap UniqueNames;
};

#endif