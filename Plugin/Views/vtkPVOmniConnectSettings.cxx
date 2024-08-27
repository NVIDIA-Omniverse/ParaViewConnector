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

#include "vtkPVOmniConnectSettings.h"

#include "vtkPVOmniConnectGlobalState.h"
#include "vtkOmniConnectLogCallback.h"

#include "vtkObjectFactory.h"

#include <cassert>
#include <cstring>

vtkSmartPointer<vtkPVOmniConnectSettings> vtkPVOmniConnectSettings::Instance;

//----------------------------------------------------------------------------
vtkPVOmniConnectSettings* vtkPVOmniConnectSettings::New()
{
  vtkPVOmniConnectSettings* instance = vtkPVOmniConnectSettings::GetInstance();
  assert(instance);
  instance->Register(NULL); // Just increases refcount signifying that client now owns a reference to the singleton as if it were a regular object (including ref decrease once done)
  return instance;
}

//----------------------------------------------------------------------------
vtkPVOmniConnectSettings* vtkPVOmniConnectSettings::GetInstance()
{
  if (!vtkPVOmniConnectSettings::Instance)
  {
    // Create and initialize the settings instance
    vtkPVOmniConnectSettings* instance = new vtkPVOmniConnectSettings();
    instance->InitializeObjectBase();
    vtkPVOmniConnectSettings::Instance.TakeReference(instance); // Keep the refcount at 1 and transfer to smartpointer
  }
  return vtkPVOmniConnectSettings::Instance;
}

//----------------------------------------------------------------------------
vtkPVOmniConnectSettings::vtkPVOmniConnectSettings()
  : OmniServer(0)
  , OmniWorkingDirectory(0)
  , LocalOutputDirectory(0)
  , OutputLocal(false)
  , OutputBinary(true)
  , ShowOmniErrors(true)
  , ShowOmniDebug(true)
  , NucleusVerbosity(0)
  , UpAxis(0)
  , UsePointInstancer(true)
  , UseStickLines(false)
  , UseStickWireframe(false)
  , UseMeshVolume(false)
  , CreateNewOmniSession(true)
{
  vtkPVOmniConnectGlobalState::GetInstance(); // Make sure a global state instance is created
}

//----------------------------------------------------------------------------
vtkPVOmniConnectSettings::~vtkPVOmniConnectSettings()
{
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectSettings::SetShowOmniErrors(int enable)
{
  if (this->ShowOmniErrors != enable)
  {
    this->ShowOmniErrors = enable;
    vtkOmniConnectLogCallback::EnableError = enable;

    this->Modified();
  }
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectSettings::SetShowOmniDebug(int enable)
{
  if(this->ShowOmniDebug != enable)
  {
    this->ShowOmniDebug = enable;
    vtkOmniConnectLogCallback::EnableDebug = enable;

    this->Modified();
  }
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectSettings::SetNucleusVerbosity(int verbosity)
{
  this->NucleusVerbosity = verbosity;
  vtkOmniConnectLogCallback::SetConnectionLogLevel(verbosity);
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectSettings::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
