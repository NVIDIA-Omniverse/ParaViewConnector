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

#ifndef vtkPVOmniConnectSettingsPlaceholder_h
#define vtkPVOmniConnectSettingsPlaceholder_h

#include "OmniConnectViewsModule.h"
#include "vtkObject.h"
#include "vtkSmartPointer.h"                      // needed for vtkSmartPointer
#include "vtkNew.h"
#include "vtkSMProxyInitializationHelper.h"

class OMNICONNECTVIEWS_EXPORT vtkPVOmniConnectSettingsPlaceholder : public vtkObject
{
public:
  static vtkPVOmniConnectSettingsPlaceholder* New();
  vtkTypeMacro(vtkPVOmniConnectSettingsPlaceholder, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent);

  /**
   * Access the singleton.
   */
  static vtkPVOmniConnectSettingsPlaceholder* GetInstance();

  static void InitializeSettings(vtkObject*, unsigned long, void* clientdata, void*);
  static void OnRegisterEvent(vtkObject*, unsigned long, void*, void*);

protected:
  vtkPVOmniConnectSettingsPlaceholder();
  ~vtkPVOmniConnectSettingsPlaceholder();

  static vtkSmartPointer<vtkPVOmniConnectSettingsPlaceholder> Instance;

private:
  vtkPVOmniConnectSettingsPlaceholder(const vtkPVOmniConnectSettingsPlaceholder&) = delete;
  void operator=(const vtkPVOmniConnectSettingsPlaceholder&) = delete;
};

#endif
