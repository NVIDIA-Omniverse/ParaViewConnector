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

#include "vtkPVOmniConnectSettingsPlaceholder.h"
#include "vtkPVOmniConnectSettings.h"

#include <cassert>

vtkSmartPointer<vtkPVOmniConnectSettingsPlaceholder> vtkPVOmniConnectSettingsPlaceholder::Instance;

//----------------------------------------------------------------------------
vtkPVOmniConnectSettingsPlaceholder* vtkPVOmniConnectSettingsPlaceholder::New()
{
  vtkPVOmniConnectSettingsPlaceholder* instance = vtkPVOmniConnectSettingsPlaceholder::GetInstance();
  assert(instance);
  instance->Register(NULL); // Just increases refcount signifying that client now owns a reference to the singleton as if it were a regular object (including ref decrease once done)
  return instance;
}

//----------------------------------------------------------------------------
vtkPVOmniConnectSettingsPlaceholder* vtkPVOmniConnectSettingsPlaceholder::GetInstance()
{
  if (!vtkPVOmniConnectSettingsPlaceholder::Instance)
  {
    // Create and initialize the settings instance
    vtkPVOmniConnectSettingsPlaceholder* instance = new vtkPVOmniConnectSettingsPlaceholder();
    instance->InitializeObjectBase();
    vtkPVOmniConnectSettingsPlaceholder::Instance.TakeReference(instance); // Keep the refcount at 1 and transfer to smartpointer
  }
  return vtkPVOmniConnectSettingsPlaceholder::Instance;
}

//----------------------------------------------------------------------------
vtkPVOmniConnectSettingsPlaceholder::vtkPVOmniConnectSettingsPlaceholder()
{
  vtkPVOmniConnectSettings::GetInstance(); // The regular settings instance will also create a global instance.
}

//----------------------------------------------------------------------------
vtkPVOmniConnectSettingsPlaceholder::~vtkPVOmniConnectSettingsPlaceholder()
{

}

//----------------------------------------------------------------------------
void vtkPVOmniConnectSettingsPlaceholder::InitializeSettings(vtkObject*, unsigned long, void*, void*)
{

}

//----------------------------------------------------------------------------
void vtkPVOmniConnectSettingsPlaceholder::OnRegisterEvent(vtkObject*, unsigned long, void* clientData, void*)
{

}

//----------------------------------------------------------------------------
void vtkPVOmniConnectSettingsPlaceholder::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
