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

#ifndef vtkPVOmniConnectSettings_h
#define vtkPVOmniConnectSettings_h

#include "OmniConnectViewsModule.h"
#include "vtkObject.h"
#include "vtkSmartPointer.h"                      // needed for vtkSmartPointer
#include "vtkNew.h"

class vtkCallbackCommand;
class vtkPVOmniConnectProxy;

class OMNICONNECTVIEWS_EXPORT vtkPVOmniConnectSettings : public vtkObject
{
public:
  static vtkPVOmniConnectSettings* New();
  vtkTypeMacro(vtkPVOmniConnectSettings, vtkObject);
  void PrintSelf(ostream& os, vtkIndent indent);

  /**
   * Access the singleton.
   */
  static vtkPVOmniConnectSettings* GetInstance();

  vtkGetStringMacro(OmniServer);
  vtkSetStringMacro(OmniServer);

  vtkGetStringMacro(OmniWorkingDirectory);
  vtkSetStringMacro(OmniWorkingDirectory);

  vtkGetStringMacro(LocalOutputDirectory);
  vtkSetStringMacro(LocalOutputDirectory);

  vtkSetMacro(OutputLocal, int);
  vtkGetMacro(OutputLocal, int);

  vtkSetMacro(OutputBinary, int);
  vtkGetMacro(OutputBinary, int);

  virtual void SetShowOmniErrors(int enable);
  vtkGetMacro(ShowOmniErrors, int);

  virtual void SetShowOmniDebug(int enable);
  vtkGetMacro(ShowOmniDebug, int);

  virtual void SetNucleusVerbosity(int verbosity);
  vtkGetMacro(NucleusVerbosity, int);

  vtkSetMacro(UpAxis, int);
  vtkGetMacro(UpAxis, int);

  vtkSetMacro(UsePointInstancer, int);
  vtkGetMacro(UsePointInstancer, int);

  vtkSetMacro(UseStickLines, int);
  vtkGetMacro(UseStickLines, int);

  vtkSetMacro(UseStickWireframe, int);
  vtkGetMacro(UseStickWireframe, int);

  vtkSetMacro(UseMeshVolume, int);
  vtkGetMacro(UseMeshVolume, int);

  vtkSetMacro(CreateNewOmniSession, int);
  vtkGetMacro(CreateNewOmniSession, int);

protected:
  vtkPVOmniConnectSettings();
  ~vtkPVOmniConnectSettings();

  char* OmniServer;
  char* OmniWorkingDirectory;
  char* LocalOutputDirectory;
  int OutputLocal;
  int OutputBinary;
  int ShowOmniErrors;
  int ShowOmniDebug;
  int NucleusVerbosity;
  int UpAxis;
  int UsePointInstancer;
  int UseStickLines;
  int UseStickWireframe;
  int UseMeshVolume;
  int CreateNewOmniSession;

  static vtkSmartPointer<vtkPVOmniConnectSettings> Instance;

private:
  vtkPVOmniConnectSettings(const vtkPVOmniConnectSettings&) = delete;
  void operator=(const vtkPVOmniConnectSettings&) = delete;
};

#endif
