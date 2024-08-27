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

#ifndef vtkOmniConnectPass_h
#define vtkOmniConnectPass_h

#include "vtkOmniverseConnectorModule.h" // For export macro
#include "vtkRenderPass.h"
#include "OmniConnectData.h"
#include "vtkNew.h"

#include <string>

class vtkOmniConnectViewNodeFactory;
class vtkOmniConnectRendererNode;

struct VTKOMNIVERSECONNECTOR_EXPORT vtkOmniConnectSettings
{
  std::string OmniServer;           // Omniverse server to connect to
  std::string OmniWorkingDirectory; // Base directory in which the PV session directories are created, containing scene + actor usd files and assets
  std::string LocalOutputDirectory; // Directory for local output (used in case OutputLocal is enabled)
  std::string RootLevelFileName;    // When set, a root level USD file will be created of the specified name, that sublayers highest level layer, with the session folder renamed to the same name.
  bool OutputLocal = true;          // Output to OmniLocalDirectory instead of the Omniverse
  bool OutputBinary = false;        // Output binary usd files instead of text-based usda
  OmniConnectAxis UpAxis = OmniConnectAxis::Y;           // Up axis of USD output
  bool UsePointInstancer = false;   // Either use UsdGeomPointInstancer for point data, or otherwise UsdGeomPoints
  bool UseStickLines = false;       // Cylinder-based stick output for line geometry (instead of curves)
  bool UseStickWireframe = false;   // Cylinder-based stick output for triangle wireframes (instead of curves)
  bool UseMeshVolume = false;       // Output textured UsdGeomMesh with MDL instead of UsdVolVolume with OpenVDBAsset fields
  bool CreateNewOmniSession = true; // Find a new Omniverse session directory on creation of the connector, or re-use the last opened one
};

class VTKOMNIVERSECONNECTOR_EXPORT vtkOmniConnectPass : public vtkRenderPass
{
public:
  static vtkOmniConnectPass* New();
  vtkTypeMacro(vtkOmniConnectPass, vtkRenderPass);
  void PrintSelf(ostream& os, vtkIndent indent) override;

  /**
   * Set up the OmniConnect
   */
  void Initialize(const vtkOmniConnectSettings& settings, const OmniConnectEnvironment& environment, double sceneTime);

  /**
   * Perform rendering according to a render state s.
   */
  virtual void Render(const vtkRenderState* s) override;

  //@{
  /**
   * Tells the pass what it will render.
   */
  void SetSceneGraph(vtkOmniConnectRendererNode*);
  vtkGetObjectMacro(SceneGraph, vtkOmniConnectRendererNode);
  //@}

  //@{
  /**
   * Set the Polydata member offset of vtkPVImageSliceMapper
   */
  void SetVtkPVImageSliceMapperPolyDataOffset(size_t offset);
  //@}

protected:
  /**
   * Default constructor.
   */
  vtkOmniConnectPass();

  /**
   * Destructor.
   */
  ~vtkOmniConnectPass() override;

  vtkNew<vtkOmniConnectViewNodeFactory> Factory;
  vtkOmniConnectRendererNode* SceneGraph = nullptr;
 
  vtkOmniConnectSettings Settings;

private:
  vtkOmniConnectPass(const vtkOmniConnectPass&) = delete;
  void operator=(const vtkOmniConnectPass&) = delete;
};

#endif
