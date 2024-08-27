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

#include "vtkOmniConnectRendererNode.h"
#include "vtkOmniConnectPass.h"
#include "vtkOmniConnectActorNode.h"
#include "vtkOmniConnectVolumeNode.h"
#include "vtkOmniConnectImageWriter.h"
#include "vtkOmniConnectLogCallback.h"
#include "vtkOmniConnectTimeStep.h"
#include "vtkObjectFactory.h"
#include "vtkRenderer.h"
#include "OmniConnect.h"
#include "vtkInformationStringKey.h"
#include "vtkPolyDataNormals.h"

#include "vtkOpenGLRenderWindow.h"
#include "vtkOpenGLState.h"
#include "vtk_glew.h"

#include <sstream>

namespace
{
  vtkOmniConnectActorNodeBase* GetActorNodeBase(vtkObject* obj)
  {
    if (vtkOmniConnectActorNode* actorNode = vtkOmniConnectActorNode::SafeDownCast(obj))
    {
      return &actorNode->ActorNodeBase;
    }
    else if (vtkOmniConnectVolumeNode* volumeNode = vtkOmniConnectVolumeNode::SafeDownCast(obj))
    {
      return &volumeNode->ActorNodeBase;
    }
    return nullptr;
  }

  void ChildrenKeepOutput(vtkViewNode* root)
  {
    auto& children = root->GetChildren();
    for (auto child : children)
    {
      vtkViewNode* viewNode = vtkViewNode::SafeDownCast(child);
      if (viewNode)
        ChildrenKeepOutput(viewNode);

      vtkOmniConnectActorNodeBase* actorNode = GetActorNodeBase(child);
      if (actorNode)
        actorNode->SetInRendererDestructor();
    }
  }
}

class vtkOmniConnectRendererNodeInternals
{
public:
  vtkOmniConnectRendererNodeInternals()
  {
    this->NormalGenerator = vtkSmartPointer<vtkPolyDataNormals>::New();
    this->NormalGenerator->ComputePointNormalsOn();
    this->NormalGenerator->ComputeCellNormalsOff();
    this->NormalGenerator->ConsistencyOff();
    this->NormalGenerator->SplittingOff();
    this->NormalGenerator->NonManifoldTraversalOff();
    this->NormalGenerator->AutoOrientNormalsOff();
    this->NormalGenerator->FlipNormalsOff();
  }

  void ResetProgressState(const std::list<vtkViewNode*>& children)
  {
    this->NumActorNodes = 0;
    for (auto child : children)
    {
      vtkViewNode* childNode = vtkViewNode::SafeDownCast(child);
      vtkOmniConnectActorNodeBase* actorNode = GetActorNodeBase(childNode);
      if (actorNode && actorNode->HasConnectorContent() && 
        actorNode->GetVtkProp3D() && actorNode->GetVtkProp3D()->GetVisibility())
      {
        this->NumActorNodes++;
      }
    }

    this->NumActorsProcessed = -1;
    this->GuiUpdated = false;
  }

  void SceneFlushAndReset(OmniConnect* connector)
  {
    if (this->DeferredSceneFlush)
      connector->FlushSceneUpdates();

    this->DeferredSceneFlush = false;
  }

  vtkSmartPointer<vtkPolyDataNormals> NormalGenerator;

  vtkOmniConnectImageWriter ImageWriter;
  vtkOmniConnectTempArrays TempArrays;
  bool DeferredSceneFlush = false;
  size_t RunningActorId = 0;
  char* DefaultProgressText = nullptr;
  int NumActorNodes = 0;
  int NumActorsProcessed = -1;
  bool GuiUpdated = false;
};

//============================================================================
vtkInformationKeyMacro(vtkOmniConnectRendererNode, ACTORNAME, String);
vtkInformationKeyMacro(vtkOmniConnectRendererNode, ACTORINPUTNAME, String);

// ----------------------------------------------------------------------------
vtkStandardNewMacro(vtkOmniConnectRendererNode);

//----------------------------------------------------------------------------
vtkOmniConnectRendererNode::vtkOmniConnectRendererNode()
  : Internals(new vtkOmniConnectRendererNodeInternals)
{ 
}

//----------------------------------------------------------------------------
vtkOmniConnectRendererNode::~vtkOmniConnectRendererNode()
{
  ChildrenKeepOutput(this);

  delete this->Connector;
  delete this->Internals;
}

//----------------------------------------------------------------------------
void vtkOmniConnectRendererNode::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
bool vtkOmniConnectRendererNode::Initialize(const vtkOmniConnectSettings& settings, const OmniConnectEnvironment& environment)
{
  if (this->Connector)
    delete this->Connector;

  vtkOmniConnectLogCallback::Callback(OmniConnectLogLevel::STATUS, nullptr, " \n");

  this->UseStickLines = settings.UseStickLines;
  this->UseStickWireframe = settings.UseStickWireframe;

  OmniConnectSettings connectSettings;
  GetOmniConnectSettings(settings, connectSettings);
  this->Connector = new OmniConnect(connectSettings, environment, vtkOmniConnectLogCallback::Callback);

  vtkOmniConnectLogCallback::Callback(OmniConnectLogLevel::STATUS, nullptr, "Initializing Omniverse Connector. \n");

  if (this->Connector->OpenConnection())
  {
    if (!settings.OutputLocal)
      vtkOmniConnectLogCallback::Callback(OmniConnectLogLevel::STATUS, nullptr, "Omniverse Connection output established. \n");
    else
      vtkOmniConnectLogCallback::Callback(OmniConnectLogLevel::STATUS, nullptr, "Usd local output established. \n");
    return true;
  }
  else
  {
    if (!settings.OutputLocal)
      vtkOmniConnectLogCallback::Callback(OmniConnectLogLevel::ERR, nullptr, "Omniverse Connection initialization error, Omniverse Connector stopping. \n");
    else
      vtkOmniConnectLogCallback::Callback(OmniConnectLogLevel::ERR, nullptr, "Usd local output initialization error, Omniverse Connector stopping. \n");
    return false;
  }
}

//----------------------------------------------------------------------------
void vtkOmniConnectRendererNode::Traverse(int operation)
{
  // Standard visit prepass, traverse children, visit postpass
  this->Superclass::Traverse(operation);
}

//----------------------------------------------------------------------------
void vtkOmniConnectRendererNode::Build(bool prepass)
{
  if (prepass)
  {
    vtkRenderer* aren = vtkRenderer::SafeDownCast(this->Renderable);
    // make sure we have a camera
    if (!aren->IsActiveCameraCreated())
    {
      aren->ResetCamera();
    }
  }
  this->Superclass::Build(prepass);
}

//----------------------------------------------------------------------------
void vtkOmniConnectRendererNode::Render(bool prepass)
{
  vtkRenderer* ren = vtkRenderer::SafeDownCast(this->GetRenderable());
  if (!ren)
  {
    return;
  }
  
  double sceneTimeStep = this->SceneTime;
  if (prepass)
  {
    this->Internals->SceneFlushAndReset(this->Connector);

    //Reset progress notifier, get the default text, and reset the internal running progress counter. 
    //Also, count the number of participating actors.
    if (this->ProgressNotifier)
    {
      this->Internals->DefaultProgressText = this->ProgressNotifier->GetProgressText();
      this->ProgressNotifier->UpdateProgress(0);

      // Reset progress internals state
      this->Internals->ResetProgressState(this->GetChildren());
    }
  }
  else
  {
    this->Internals->SceneFlushAndReset(this->Connector);

    if (this->ProgressNotifier)
    { 
      this->ProgressNotifier->UpdateProgress(0.0);
      this->ProgressNotifier->SetProgressText(this->Internals->DefaultProgressText);
      this->Internals->DefaultProgressText = nullptr;

      // Gui updates cause untracked Qt OpenGL changes, disturbing state.
      if (this->Internals->GuiUpdated)
        this->ResetOpenGLState();
    }

    this->ResetMaterialPolicyChanged();
    this->ResetPolyDataPolicyChanged();
  }
}

//------------------------------------------------------------------------------
vtkRenderer* vtkOmniConnectRendererNode::GetRenderer()
{
  return vtkRenderer::SafeDownCast(this->GetRenderable());
}

//------------------------------------------------------------------------------
void vtkOmniConnectRendererNode::SetMergeTfIntoVol(bool enable)
{
  this->MaterialPolicyChanged = this->MergeTfIntoVol != enable;
  this->MergeTfIntoVol = enable;
}

//------------------------------------------------------------------------------
bool vtkOmniConnectRendererNode::GetMergeTfIntoVol() const
{
  return this->MergeTfIntoVol;
}

//------------------------------------------------------------------------------
void vtkOmniConnectRendererNode::SetForceTimeVaryingTextures(bool enable)
{
  this->MaterialPolicyChanged = this->ForceTimeVaryingTextures != enable;
  this->ForceTimeVaryingTextures = enable;
}

//------------------------------------------------------------------------------
bool vtkOmniConnectRendererNode::GetForceTimeVaryingTextures() const
{
  return this->ForceTimeVaryingTextures;
}

//------------------------------------------------------------------------------
void vtkOmniConnectRendererNode::SetForceTimeVaryingMaterialParams(bool enable)
{
  this->MaterialPolicyChanged = this->ForceTimeVaryingMaterialParams != enable;
  this->ForceTimeVaryingMaterialParams = enable;
}

//------------------------------------------------------------------------------
bool vtkOmniConnectRendererNode::GetForceTimeVaryingMaterialParams() const
{
  return this->ForceTimeVaryingMaterialParams;
}

//------------------------------------------------------------------------------
void vtkOmniConnectRendererNode::SetForceConsistentWinding(bool enable)
{
  this->PolyDataPolicyChanged = this->ForceConsistentWinding != enable;
  this->ForceConsistentWinding = enable;
}

//------------------------------------------------------------------------------
bool vtkOmniConnectRendererNode::GetForceConsistentWinding() const
{
  return this->ForceConsistentWinding;
}

//------------------------------------------------------------------------------
void vtkOmniConnectRendererNode::SetAllowWidgetActors(bool enable)
{
  this->AllowWidgetActors = enable;
}

//------------------------------------------------------------------------------
bool vtkOmniConnectRendererNode::GetAllowWidgetActors() const
{
  return this->AllowWidgetActors;
}

//------------------------------------------------------------------------------
void vtkOmniConnectRendererNode::SetLiveWorkflowEnabled(bool enable)
{
  this->LiveWorkflowEnabled = enable;
  if(this->Connector)
    this->Connector->SetLiveWorkflowEnabled(enable);
}

//------------------------------------------------------------------------------
bool vtkOmniConnectRendererNode::GetLiveWorkflowEnabled() const
{
  return this->LiveWorkflowEnabled;
}

//------------------------------------------------------------------------------
vtkOmniConnectRendererNode* vtkOmniConnectRendererNode::GetRendererNode(vtkViewNode* self)
{
  return static_cast<vtkOmniConnectRendererNode*>(self->GetFirstAncestorOfType("vtkOmniConnectRendererNode"));
}

//------------------------------------------------------------------------------
void vtkOmniConnectRendererNode::FlushSceneUpdates()
{
  this->Internals->DeferredSceneFlush = true;
}

//------------------------------------------------------------------------------
void vtkOmniConnectRendererNode::SetProgressNotifier(vtkAlgorithm* progressNotifier)
{
  this->ProgressNotifier = progressNotifier;
}

//------------------------------------------------------------------------------
void vtkOmniConnectRendererNode::SetProgressText(const char* progressText)
{
  if(this->ProgressNotifier)
    this->ProgressNotifier->SetProgressText(progressText);
}

//------------------------------------------------------------------------------
void vtkOmniConnectRendererNode::SetActorProgress(double actorPercentage, bool showInGui)
{
  if (actorPercentage == 0.0)
    this->Internals->NumActorsProcessed++;
  
  double percentageActorsComplete = (double)this->Internals->NumActorsProcessed / (double)this->Internals->NumActorNodes;
  if (showInGui && this->ProgressNotifier)
  {
    this->ProgressNotifier->UpdateProgress(percentageActorsComplete + actorPercentage);
    this->Internals->GuiUpdated = true;
  }
}

//------------------------------------------------------------------------------
vtkOmniConnectImageWriter* vtkOmniConnectRendererNode::GetImageWriter()
{
  return &this->Internals->ImageWriter;
}

//------------------------------------------------------------------------------
vtkPolyDataNormals* vtkOmniConnectRendererNode::GetNormalGenerator()
{
  return this->Internals->NormalGenerator.Get();
}

//------------------------------------------------------------------------------
vtkOmniConnectTempArrays& vtkOmniConnectRendererNode::GetTempArrays()
{
  return this->Internals->TempArrays;
}

//------------------------------------------------------------------------------
size_t vtkOmniConnectRendererNode::NewActorId()
{
  return ++this->Internals->RunningActorId;
}

//------------------------------------------------------------------------------
void vtkOmniConnectRendererNode::CheckOpenGLState()
{
  vtkOpenGLState* ostate = this->OpenGLWindow->GetState();
  ostate->GetEnumState(0);
}

//------------------------------------------------------------------------------
void vtkOmniConnectRendererNode::ResetOpenGLState()
{
  // Make sure we're resetting (reading back) GL state from the appropriate context.
  this->OpenGLWindow->MakeCurrent();

  vtkOpenGLState* ostate = this->OpenGLWindow->GetState();
  ostate->ResetGLClearColorState();
  ostate->ResetGLClearDepthState();
  ostate->ResetGLDepthFuncState();
  ostate->ResetGLDepthMaskState();
  ostate->ResetGLColorMaskState();
  ostate->ResetGLViewportState();
  ostate->ResetGLScissorState();
  ostate->ResetGLBlendFuncState();
  ostate->ResetGLBlendEquationState();
  ostate->ResetGLCullFaceState();
  ostate->ResetGLActiveTexture();
  ostate->ResetEnumState(GL_BLEND);
  ostate->ResetEnumState(GL_DEPTH_TEST);
  ostate->ResetEnumState(GL_CULL_FACE);
  ostate->ResetEnumState(GL_MULTISAMPLE);
  ostate->ResetEnumState(GL_SCISSOR_TEST);
  ostate->ResetEnumState(GL_STENCIL_TEST);
}


