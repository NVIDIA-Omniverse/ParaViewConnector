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

#ifndef vtkOmniConnectRendererNode_h
#define vtkOmniConnectRendererNode_h

#include "vtkOmniverseConnectorModule.h" // For export macro
#include "vtkRendererNode.h"

class vtkRenderer;
class vtkOpenGLRenderWindow;
class OmniConnect;
struct OmniConnectEnvironment;
class vtkOmniConnectRendererNodeInternals;
class vtkOmniConnectImageWriter;
class vtkInformationStringKey;
class vtkPolyDataNormals;
class vtkAlgorithm;
struct vtkOmniConnectSettings;
struct vtkOmniConnectTempArrays;


class VTKOMNIVERSECONNECTOR_EXPORT vtkOmniConnectRendererNode : public vtkRendererNode
{
public:
  static vtkOmniConnectRendererNode* New();
  vtkTypeMacro(vtkOmniConnectRendererNode, vtkRendererNode);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * Initialize Omniverse Connector.
   */
  bool Initialize(const vtkOmniConnectSettings& settings, const OmniConnectEnvironment& environment);

  /**
   * Builds myself.
   */
  void Build(bool prepass) override;

  /**
   * Graph render
   */
  void Render(bool prepass) override;

  /**
   * Traverse graph
   */
  void Traverse(int operation) override;

  /**
   * Get the Omniverse Connector
   */
  OmniConnect* GetOmniConnector() { return this->Connector; }

  /**
  * New Actor Id
  */
  size_t NewActorId();

  /**
  * Parameters that can be changed on the fly
  */
  vtkSetMacro(RenderingEnabled, bool);
  vtkGetMacro(RenderingEnabled, bool);

  vtkSetMacro(SceneTime, double);
  vtkGetMacro(SceneTime, double);

  vtkSetMacro(UseStickLines, bool);
  vtkGetMacro(UseStickLines, bool);

  vtkSetMacro(UseStickWireframe, bool);
  vtkGetMacro(UseStickWireframe, bool);

  // Material policies (should be per-actor, but lack of functionality forces per-view storage)
  void SetMergeTfIntoVol(bool enable); // Pre-classified volume output (OpenVDB), resulting in rgba color-volume ovdb data
  bool GetMergeTfIntoVol() const;

  void SetForceTimeVaryingTextures(bool enable); // Texture files are replicated for every timestep, instead of having one for all timesteps
  bool GetForceTimeVaryingTextures() const;

  void SetForceTimeVaryingMaterialParams(bool enable); // Material parameters are replicated for every timestep, instead of having one for all timesteps
  bool GetForceTimeVaryingMaterialParams() const;

  // PolyData policies
  void SetForceConsistentWinding(bool enable); // Forces correct winding order on polydata (via the normalgenerator)
  bool GetForceConsistentWinding() const;

  // Actor policies
  void SetAllowWidgetActors(bool enable);
  bool GetAllowWidgetActors() const;

  // Live workflow
  void SetLiveWorkflowEnabled(bool enable);
  bool GetLiveWorkflowEnabled() const;

  // Reset/Get for policy categories
  void ResetMaterialPolicyChanged() { MaterialPolicyChanged = false; }
  bool GetMaterialPolicyChanged() const { return MaterialPolicyChanged; }

  void ResetPolyDataPolicyChanged() { PolyDataPolicyChanged = false; }
  bool GetPolyDataPolicyChanged() const { return PolyDataPolicyChanged; }

  /**
   * Convenience method to get and downcast renderable.
   */
  static vtkOmniConnectRendererNode* GetRendererNode(vtkViewNode*);
  vtkRenderer* GetRenderer();

  /**
   * Allow Rendernode to flush the scene when appropriate.
   */
  void FlushSceneUpdates();

  /**
   * Set the progress notifier
   */
  void SetProgressNotifier(vtkAlgorithm* progressNotifier);
  void SetProgressText(const char* progressText);
  void SetActorProgress(double actorPercentage, bool showInGui);

  /**
   * Helper structures for common operations.
   */
  vtkOmniConnectImageWriter* GetImageWriter();
  vtkPolyDataNormals* GetNormalGenerator();
  vtkOmniConnectTempArrays& GetTempArrays();
  
  static vtkInformationStringKey* ACTORNAME();
  static vtkInformationStringKey* ACTORINPUTNAME();

  void SetOpenGLWindow(vtkOpenGLRenderWindow* win) { this->OpenGLWindow = win; }
  void CheckOpenGLState();
  void ResetOpenGLState();

protected:
  vtkOmniConnectRendererNode();
  ~vtkOmniConnectRendererNode() override;

  OmniConnect* Connector = nullptr;
  vtkOmniConnectRendererNodeInternals* Internals = nullptr;

  bool RenderingEnabled = true;
  double SceneTime = 0;
  bool UseStickLines = false;
  bool UseStickWireframe = false;

  bool MergeTfIntoVol = false;
  bool ForceTimeVaryingTextures = false;
  bool ForceTimeVaryingMaterialParams = false;
  bool ForceConsistentWinding = false;
  bool AllowWidgetActors = false;
  bool LiveWorkflowEnabled = false;

  bool MaterialPolicyChanged = false;
  bool PolyDataPolicyChanged = false;

  vtkAlgorithm* ProgressNotifier = nullptr;

  vtkOpenGLRenderWindow* OpenGLWindow;

private:
  vtkOmniConnectRendererNode(const vtkOmniConnectRendererNode&) = delete;
  void operator=(const vtkOmniConnectRendererNode&) = delete;
};

#endif
