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

#ifndef vtkPVOmniConnectRenderView_h
#define vtkPVOmniConnectRenderView_h

#include "OmniConnectViewsModule.h"
#include "vtkNew.h" // for vtkNew
#include "vtkPVRenderView.h"

class vtkPVOmniConnectRenderViewBehavior;
struct vtkOmniConnectSettings;
class vtkOmniConnectPass;
class vtkExecutive;
class vtkPVSessionCore;

#include <vector>

class OMNICONNECTVIEWS_EXPORT vtkPVOmniConnectRenderView : public vtkPVRenderView
{
public:
  static vtkPVOmniConnectRenderView* New();
  vtkTypeMacro(vtkPVOmniConnectRenderView, vtkPVRenderView);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Overrides connection settings in settings file, call before Initialize()
  void SetExplicitConnectionSettings(vtkOmniConnectSettings& connectionSettings);
  static void GetSettingsFromGlobalInstance(vtkOmniConnectSettings& settings);

  // Initialize
  void Initialize();

  virtual void SetRenderingEnabled(bool);
  vtkGetMacro(RenderingEnabled, bool);
  vtkBooleanMacro(RenderingEnabled, bool);

  virtual void SetForceTimeVaryingTextures(bool);
  vtkGetMacro(ForceTimeVaryingTextures, bool);
  vtkBooleanMacro(ForceTimeVaryingTextures, bool);

  virtual void SetForceTimeVaryingMaterialParams(bool);
  vtkGetMacro(ForceTimeVaryingMaterialParams, bool);
  vtkBooleanMacro(ForceTimeVaryingMaterialParams, bool);

  virtual void SetForceConsistentWinding(bool);
  vtkGetMacro(ForceConsistentWinding, bool);
  vtkBooleanMacro(ForceConsistentWinding, bool);

  virtual void SetMergeTfIntoVol(bool);
  vtkGetMacro(MergeTfIntoVol, bool);
  vtkBooleanMacro(MergeTfIntoVol, bool);

  virtual void SetAllowWidgetActors(bool);
  vtkGetMacro(AllowWidgetActors, bool);
  vtkBooleanMacro(AllowWidgetActors, bool);

  virtual void SetLiveWorkflowEnabled(bool);
  vtkGetMacro(LiveWorkflowEnabled, bool);
  vtkBooleanMacro(LiveWorkflowEnabled, bool);

  virtual void SetGlobalTimeScale(double);
  vtkGetMacro(GlobalTimeScale, double);

  void Update() override;
  void Render(bool interactive, bool skip_rendering) override;
  void SetViewTime(double value) override;
  void SetRemoteRenderingThreshold(double threshold) override;
  void SetLODRenderingThreshold(double threshold) override;

protected:
  vtkPVOmniConnectRenderView();
  ~vtkPVOmniConnectRenderView();

  // To be able to find out which actors influenced by the animation scene
  // and therefore require the animation timestep set as data timestep.
  void ReceiveAnimatedObjects();
  void UpdateActorNamesFromProxy();
  bool HasAnimatedParent(vtkExecutive* exec);

  bool Initialized = false;

  bool RenderingEnabled = true;
  bool ForceTimeVaryingTextures = false;
  bool ForceTimeVaryingMaterialParams = false;
  bool ForceConsistentWinding = false;
  bool MergeTfIntoVol = false;
  bool LiveWorkflowEnabled = false;
  bool AllowWidgetActors = false;
  double GlobalTimeScale = 1.0;

  double ViewTime = 0.0;

  vtkOmniConnectSettings* ExplicitConnectionSettings = nullptr;

  bool IsClientOnly = false; 
  vtkNew<vtkOmniConnectPass> OmniConnectPass;

  std::vector<vtkTypeUInt32> AnimatedObjectIDs;
  std::vector<vtkObjectBase*> AnimatedObjects;

private:
  vtkPVOmniConnectRenderView(const vtkPVOmniConnectRenderView&); // Not implemented
  void operator=(const vtkPVOmniConnectRenderView&); // Not implemented
};

#endif
