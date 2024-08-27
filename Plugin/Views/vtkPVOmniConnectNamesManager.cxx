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

#include "vtkPVOmniConnectNamesManager.h"

#include "vtkOmniConnectRendererNode.h"
#include "OmniConnectUtilsExternal.h"
#include "vtkInformation.h"
#include "vtkSIObject.h"
#include "vtkSIProxy.h"
#include "vtkPVSessionCore.h"
#include "vtkGeometryRepresentation.h"
#include "vtkImageSliceRepresentation.h"
#include "vtkImageVolumeRepresentation.h"
#include "vtkPVLODActor.h"

typedef vtkOmniConnectRendererNode OmniInformationBase;


//----------------------------------------------------------------------------
vtkPVOmniConnectNamesManager::vtkPVOmniConnectNamesManager()
{

}

//----------------------------------------------------------------------------
vtkPVOmniConnectNamesManager::~vtkPVOmniConnectNamesManager()
{

}

//----------------------------------------------------------------------------
const char* vtkPVOmniConnectNamesManager::GetUniqueName(vtkView* view, std::string& baseName)
{
  NameSet& uniqueViewNames = this->UniqueNames[view];

  const char* empRes = nullptr;
  if (NameIsUnique(baseName, uniqueViewNames))
  {
    empRes = uniqueViewNames.emplace(baseName).first->c_str();
  }
  else
  {
    int postFix = 1;
    std::string proposedBaseName(baseName+"_");
    std::string proposedName(proposedBaseName + std::to_string(postFix));

    while (!NameIsUnique(proposedName, uniqueViewNames))
    {
      ++postFix;
      proposedName = proposedBaseName + std::to_string(postFix);
    }
    empRes = uniqueViewNames.emplace(proposedName).first->c_str();
  }
  return empRes;
}

//----------------------------------------------------------------------------
const char* vtkPVOmniConnectNamesManager::GetSurfaceName(vtkView* view, std::string& baseName)
{
  // Return a stored string that ends in "Surface" (ie. a surface representation)
  // which USD-matches the baseName up to the length of baseName
  NameSet& uniqueViewNames = this->UniqueNames[view];

  const char* surfaceStr = "Surface";
  size_t surfaceStrLen = strlen(surfaceStr);

  for(const auto& storedName : uniqueViewNames)
  {
    const char* storedCstr = storedName.c_str();
    size_t storedNameLen = storedName.length();
    if(storedNameLen > surfaceStrLen)
    {
      const char* storedStrRepr = storedCstr + storedNameLen - surfaceStrLen;

      if(ocutils::UsdNamesEqual(baseName.c_str(), storedCstr, baseName.length()) &&
        strcmp(storedStrRepr, surfaceStr) == 0)
        return storedCstr;
    }
  }

  return baseName.c_str();
}

//----------------------------------------------------------------------------
bool vtkPVOmniConnectNamesManager::NameIsUnique(std::string& name, const vtkPVOmniConnectNamesManager::NameSet& uniqueViewNames) const
{
  for(const auto& storedName : uniqueViewNames)
  {
    if(ocutils::UsdNamesEqual(name.c_str(), storedName.c_str()))
      return false;
  }

  return true;
}

//----------------------------------------------------------------------------
vtkProp3D* vtkPVOmniConnectNamesManager::GetActorFromRepresentation(vtkDataRepresentation* dataRepr)
{
  vtkProp3D* actor;
  vtkGeometryRepresentation* geomRepr = vtkGeometryRepresentation::SafeDownCast(dataRepr);
  if (!geomRepr)
  {
    vtkImageSliceRepresentation* sliceRepr = vtkImageSliceRepresentation::SafeDownCast(dataRepr);
    if (!sliceRepr)
    {
      vtkImageVolumeRepresentation* volRepr = vtkImageVolumeRepresentation::SafeDownCast(dataRepr);
      if (!volRepr)
        actor = nullptr;
      else
        actor = volRepr->GetActor();
    }
    else
      actor = sliceRepr->GetActor();
  }
  else
  {
    actor = geomRepr->GetActor();
  }
  return actor;
}

//----------------------------------------------------------------------------
vtkInformation* vtkPVOmniConnectNamesManager::GetOrCreatePropertyKeys(vtkProp3D* actor)
{
  vtkInformation* actInfo = actor->GetPropertyKeys();
  if (!actInfo)
  {
    actInfo = vtkInformation::New();
    actor->SetPropertyKeys(actInfo);
    actInfo->Delete();
  }
  return actInfo;
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectNamesManager::ClearActorInputName(vtkInformation* actInfo, vtkPVSessionCore* sessionCore)
{
  if(sessionCore)
  {
    // Check if (suggested) name has already been set; return if so
    if(actInfo->Has(OmniInformationBase::ACTORINPUTNAME()))
    {
      actInfo->Remove(OmniInformationBase::ACTORINPUTNAME());
    }
  }
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectNamesManager::SetActorName(vtkView* view, vtkPVDataRepresentation* dataRepr, vtkInformation* actInfo, const char* suggestedName,
  vtkPVSessionCore* sessionCore, bool isInputActorName)
{
  vtkInformationStringKey* nameKey = isInputActorName ? OmniInformationBase::ACTORINPUTNAME() : OmniInformationBase::ACTORNAME();

  if(sessionCore)
  {
    // Check if (suggested) name has already been set; return if so
    if(actInfo->Has(nameKey))
    {
      const char* existingName = actInfo->Get(nameKey);
      if(!suggestedName || strncmp(suggestedName, existingName, STRNLEN_PORTABLE(suggestedName, MAX_USER_STRLEN)) == 0)
          return;
    }

    unsigned int dataReprId = dataRepr->GetUniqueIdentifier();
    vtkSIObject* siObj = sessionCore->GetSIObject(dataReprId);
    if(!siObj)
      return;
    vtkSIProxy* siProxy = vtkSIProxy::SafeDownCast(siObj);

    // Get the representation base name and the representation type string
    const char* siReprName = siProxy->GetLogNameOrDefault();
    const char* reprStrConst = "Representation";
    size_t reprStrLen = strlen(reprStrConst);
    const char* siReprNameEnd = strstr(siReprName, reprStrConst);

    std::string coreName;
    if (!isInputActorName && siReprNameEnd)
    {
      // Use the representation type as a postfix to the name
      // Generally, siReprName is assumed to be '<DataType>#<reprStrConst>/<Representation><reprStrConst>'
      // So search for the start of the second reprStrConst as the end, and the / as the start of the actual
      // representation name. 
      const char* reprTypeStr = siReprNameEnd ? siReprNameEnd + reprStrLen : nullptr;
      const char* reprTypeStrEnd = reprTypeStr ? strstr(reprTypeStr, reprStrConst) : nullptr;
      while (reprTypeStr != reprTypeStrEnd && *reprTypeStr != '/')
        ++reprTypeStr;

      if(suggestedName)
        coreName.assign(suggestedName);
      else
        coreName.assign(siReprName, siReprNameEnd);

      if(reprTypeStrEnd != reprTypeStr)
      {
        // reprTypeStr is still on the '/', but that will automatically be replaced with '_'
        coreName.append(reprTypeStr, reprTypeStrEnd - reprTypeStr);
      }
    }
    else
    {
      coreName.assign(suggestedName ? suggestedName : siReprName);
    }

    actInfo->Set(nameKey, !isInputActorName ? GetUniqueName(view, coreName) : GetSurfaceName(view, coreName)); // only create unique names for OmniInformationBase::ACTORNAME keys
  }
  else
  {
    if (!actInfo->Has(nameKey))
      actInfo->Set(nameKey, "");
  }
}