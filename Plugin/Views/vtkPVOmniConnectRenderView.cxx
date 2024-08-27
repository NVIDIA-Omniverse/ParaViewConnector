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

#include "vtkPVOmniConnectRenderView.h"

#include "vtkOmniConnectPass.h"
#include "vtkOmniConnectRendererNode.h"
#include "vtkOmniConnectGlyph3DMapperNode.h"
#include "vtkOmniConnectLogCallback.h"
#include "vtkPVOmniConnectSettings.h"
#include "vtkPVOmniConnectGlobalState.h"
#include "vtkPVOmniConnectAnimProxy.h"
#include "vtkPVOmniConnectNamesManager.h"

#include "vtkInformation.h"
#include "vtkMultiProcessController.h"
#include "vtkPVLODActor.h"
#include "vtkPVLODVolume.h"
#include "vtkPVSession.h"
#include "vtkProcessModule.h"
#include "vtkPVSessionCore.h"
#include "vtkPVSessionBase.h"
#include "vtkSIProxy.h"
#include "vtkExecutive.h"
#include "vtkDataObject.h"
#include "vtkPVImageSliceMapper.h"
#include "vtkRenderWindow.h"
#include "vtkRendererCollection.h"
#include "vtkSphereSource.h"
#include "vtkConeSource.h"
#include "vtkCylinderSource.h"
#include "vtkCubeSource.h"
#include "vtkArrowSource.h"
#include "vtkLineSource.h"
#include "vtkGlyphSource2D.h"
#include "vtkPVSynchronizedRenderer.h"
#include "vtkGeometryRepresentation.h"

#include <cassert>
#include <cstring>
#include <algorithm>

vtkStandardNewMacro(vtkPVOmniConnectRenderView);


namespace
{
  void AddGlyphSourcePropertyKeys(vtkDataRepresentation* dataRepr, vtkInformation* actInfo)
  {
    vtkGeometryRepresentation* geomRepr = vtkGeometryRepresentation::SafeDownCast(dataRepr);
    if (geomRepr)
    {
      vtkExecutive* geomReprExec = geomRepr->GetExecutive();
      if(geomReprExec->GetNumberOfInputPorts() > 1)
      {
        vtkExecutive* inExec = geomReprExec->GetInputExecutive(1, 0);

        vtkExecutive* inExec2 = nullptr;
        vtkAlgorithm* sourceAlg = nullptr;
        if(inExec && inExec->GetNumberOfInputPorts() > 0)
        {
          inExec2 = inExec->GetInputExecutive(0, 0);

          if(inExec2)
          {
            double scale[3] = {0.5, 0.5, 0.5};
            sourceAlg = inExec2->GetAlgorithm();
            if(vtkConeSource* source = vtkConeSource::SafeDownCast(sourceAlg))
            {
              actInfo->Set(vtkOmniConnectGlyph3DMapperNode::GLYPHSHAPE(), vtkOmniConnectGlyph3DMapperNode::SHAPE_CONE);

              scale[0] = source->GetHeight()*0.5;
              scale[1] = source->GetRadius();
            }
            else if(vtkCylinderSource* source = vtkCylinderSource::SafeDownCast(sourceAlg))
            {
              actInfo->Set(vtkOmniConnectGlyph3DMapperNode::GLYPHSHAPE(), vtkOmniConnectGlyph3DMapperNode::SHAPE_CYLINDER);

              scale[0] = source->GetHeight()*0.5;
              scale[1] = source->GetRadius();
            }
            else if(vtkCubeSource* source = vtkCubeSource::SafeDownCast(sourceAlg))
            {
              actInfo->Set(vtkOmniConnectGlyph3DMapperNode::GLYPHSHAPE(), vtkOmniConnectGlyph3DMapperNode::SHAPE_CUBE);

              scale[0] = source->GetXLength()*0.5;
              scale[1] = source->GetYLength()*0.5;
              scale[2] = source->GetZLength()*0.5;
            }
            else if(vtkArrowSource* source = vtkArrowSource::SafeDownCast(sourceAlg))
            {
              actInfo->Set(vtkOmniConnectGlyph3DMapperNode::GLYPHSHAPE(), vtkOmniConnectGlyph3DMapperNode::SHAPE_ARROW);

              scale[0] = source->GetTipLength();
              scale[1] = source->GetShaftRadius();
              scale[2] = source->GetTipRadius();
            }
            else if(vtkLineSource* source = vtkLineSource::SafeDownCast(sourceAlg))
            {
              actInfo->Set(vtkOmniConnectGlyph3DMapperNode::GLYPHSHAPE(), vtkOmniConnectGlyph3DMapperNode::SHAPE_ARROW);

              scale[0] = 0;
              scale[1] = 0.01;
              scale[2] = 0.01;
            }
            else if(vtkGlyphSource2D* source = vtkGlyphSource2D::SafeDownCast(sourceAlg))
            {
              actInfo->Set(vtkOmniConnectGlyph3DMapperNode::GLYPHSHAPE(), vtkOmniConnectGlyph3DMapperNode::SHAPE_ARROW);

              scale[0] = source->GetTipLength();
              scale[1] = 0.01;
              scale[2] = 0.03;
            }
            else if(!vtkSphereSource::SafeDownCast(sourceAlg) &&
              actInfo->Has(vtkOmniConnectRendererNode::ACTORINPUTNAME()))
            {
              actInfo->Set(vtkOmniConnectGlyph3DMapperNode::GLYPHSHAPE(), vtkOmniConnectGlyph3DMapperNode::SHAPE_EXTERNAL);
            }
            else // Sphere is default
            {
              actInfo->Set(vtkOmniConnectGlyph3DMapperNode::GLYPHSHAPE(), vtkOmniConnectGlyph3DMapperNode::SHAPE_SPHERE);

              if(vtkSphereSource* source = vtkSphereSource::SafeDownCast(sourceAlg))
              {
                scale[0] = source->GetRadius();
              }
            }
            actInfo->Set(vtkOmniConnectGlyph3DMapperNode::GLYPHDIMS(), scale, 3);
          }
        }
      }
    }
  }
}

// Accessor classes
class vtkImageSliceMapperAccessor : public vtkPVImageSliceMapper
{
public:
  static size_t GetPolyDataActorOffset() 
  { 
    vtkPVImageSliceMapper* mapper = vtkPVImageSliceMapper::New();
    vtkActor* vtkPVImageSliceMapper::* memberPtr = &vtkImageSliceMapperAccessor::PolyDataActor; 
    size_t offset = (char*)(&(mapper->*memberPtr)) - (char*)mapper;
    mapper->Delete();
    return offset;
  }
};

//----------------------------------------------------------------------------
vtkPVOmniConnectRenderView::vtkPVOmniConnectRenderView()
{
  this->OmniConnectPass->SetVtkPVImageSliceMapperPolyDataOffset(vtkImageSliceMapperAccessor::GetPolyDataActorOffset());

  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  if (pm->GetPartitionId() > 0)
  {
    return;
  }

  vtkPVSession* session = vtkPVSession::SafeDownCast(pm->GetSession());
  if (session && !session->HasProcessRole(vtkPVSession::RENDER_SERVER))
  {
    // Pure client won't initialize the omniverse connection pass 
    IsClientOnly = true;
  }
}

//----------------------------------------------------------------------------
vtkPVOmniConnectRenderView::~vtkPVOmniConnectRenderView()
{
  //this->UnRegisterProgress(this);
  delete ExplicitConnectionSettings;
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectRenderView::SetExplicitConnectionSettings(vtkOmniConnectSettings& connectionSettings)
{
  if(!ExplicitConnectionSettings)
    ExplicitConnectionSettings = new vtkOmniConnectSettings();

  *ExplicitConnectionSettings = connectionSettings;
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectRenderView::GetSettingsFromGlobalInstance(vtkOmniConnectSettings& settings)
{
  vtkPVOmniConnectSettings* omniSettings = vtkPVOmniConnectSettings::GetInstance();

  std::string omniServerStr; if(omniSettings->GetOmniServer()) omniServerStr = omniSettings->GetOmniServer();
  std::string omniWorkingDirectoryStr; if(omniSettings->GetOmniWorkingDirectory()) omniWorkingDirectoryStr = omniSettings->GetOmniWorkingDirectory();
  std::string omniLocalOutputDirectoryStr; if(omniSettings->GetLocalOutputDirectory()) omniLocalOutputDirectoryStr = omniSettings->GetLocalOutputDirectory(); 
  std::string rootLevelFileNameStr;

  settings = {
    omniServerStr,
    omniWorkingDirectoryStr,
    omniLocalOutputDirectoryStr,
    rootLevelFileNameStr,
    (bool)omniSettings->GetOutputLocal(),
    (bool)omniSettings->GetOutputBinary(),
    (omniSettings->GetUpAxis() == 0) ? OmniConnectAxis::Y : OmniConnectAxis::Z,
    (bool)omniSettings->GetUsePointInstancer(),
    (bool)omniSettings->GetUseStickLines(),
    (bool)omniSettings->GetUseStickWireframe(),
    (bool)omniSettings->GetUseMeshVolume(),
    (bool)omniSettings->GetCreateNewOmniSession()
  };
}

//----------------------------------------------------------------------------
void vtkPVOmniConnectRenderView::Initialize()
{
  Initialized = true;

  if (!this->SynchronizedRenderers)
    return;

  vtkOmniConnectSettings configSettings;
  GetSettingsFromGlobalInstance(configSettings);

  vtkOmniConnectSettings chosenSettings = ExplicitConnectionSettings ? *ExplicitConnectionSettings : configSettings;

  vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
  OmniConnectEnvironment omniEnv{ pm->GetPartitionId(), pm->GetNumberOfLocalPartitions() };

  vtkMultiProcessController* controller = vtkMultiProcessController::GetGlobalController();
  if (controller && omniEnv.ProcId > 0)
  {
    chosenSettings.CreateNewOmniSession = false;

    // For all proc's != 0, wait with renderer initialization until proc_0's is complete.
    int DummyData = 0;
    controller->Broadcast(&DummyData, 1, 0);

    vtkOmniConnectLogCallback::Callback(OmniConnectLogLevel::STATUS, nullptr, "Omniverse log: Proc > 0 starting initialization.");
  }
  
  this->OmniConnectPass->Initialize(chosenSettings, omniEnv, ViewTime);

  this->SynchronizedRenderers->SetRenderPass(this->OmniConnectPass.GetPointer());

  // Send a signal from proc 0 to the other processes to also initialize their renderer
  if (controller && omniEnv.NumProcs > 1 && omniEnv.ProcId == 0)
  {
    int DummyData = 0;
    controller->Broadcast(&DummyData, 1, 0);

    vtkOmniConnectLogCallback::Callback(OmniConnectLogLevel::STATUS, nullptr, "Omniverse log: Proc 0 finished initialization.");
  }
}

//void vtkPVOmniConnectRenderView::StillRender()
//{
//  SetRendererWindowActive();
//
//  this->Superclass::StillRender();
//}
//
//void vtkPVOmniConnectRenderView::InteractiveRender()
//{
//  SetRendererWindowActive();
//
//  this->Superclass::InteractiveRender();
//}

void vtkPVOmniConnectRenderView::ReceiveAnimatedObjects()
{
  // has to be done via explicit global state, since on the server there is no proxy manager to get the animation proxy from
  vtkPVOmniConnectAnimProxy* omniConnectAnimProxy = vtkPVOmniConnectGlobalState::GetInstance()->GetOmniConnectAnimProxy();
  if(omniConnectAnimProxy)
  {
    omniConnectAnimProxy->GetAnimatedObjects(this->AnimatedObjects);
  }
}

void vtkPVOmniConnectRenderView::UpdateActorNamesFromProxy()
{
  // has to be done via explicit global state, since on the server there is no proxy manager to get the animation proxy from
  vtkPVOmniConnectAnimProxy* omniConnectAnimProxy = vtkPVOmniConnectGlobalState::GetInstance()->GetOmniConnectAnimProxy();
  if(omniConnectAnimProxy)
  {
    omniConnectAnimProxy->UpdateActorInfoFromRegisteredObjectNames();
  }
}

bool vtkPVOmniConnectRenderView::HasAnimatedParent(vtkExecutive* exec)
{
  vtkObjectBase* alg = exec->GetAlgorithm();
  bool animParentFound = alg && (std::find(this->AnimatedObjects.begin(), this->AnimatedObjects.end(), alg) != this->AnimatedObjects.end());

  for(int port = 0; port < exec->GetNumberOfInputPorts(); ++port)
  {
    for(int conn = 0; conn < exec->GetNumberOfInputConnections(port); ++conn)
    {
      animParentFound = animParentFound || HasAnimatedParent(exec->GetInputExecutive(port, conn));
    }
  }

  return animParentFound;
}

void vtkPVOmniConnectRenderView::Update()
{
  if (!IsClientOnly && !Initialized)
    Initialize();

  // ------ This seems to not be necessary anymore, since PV5.11 ------
  // Make sure the pure client representations have an actorname property. Otherwise, the synchronization with the server will crash on mismatching tags.
  //if (IsClientOnly)
  //{
  //  int num_reprs = this->GetNumberOfRepresentations();
  //  for (int i = 0; i < num_reprs; ++i)
  //  {
  //    vtkProp3D* actor = nullptr;
  //    vtkPVDataRepresentation* dataRepr = vtkPVDataRepresentation::SafeDownCast(this->GetRepresentation(i));
  //    if (dataRepr)
  //    {
  //      actor = vtkPVOmniConnectNamesManager::GetActorFromRepresentation(dataRepr);
  //      if (!actor)
  //        continue;
  //    }
  //    else
  //      continue;
  //
  //    vtkInformation* info = vtkPVOmniConnectNamesManager::GetOrCreatePropertyKeys(actor);
  //
  //    vtkPVOmniConnectNamesManager& namesManager = vtkPVOmniConnectGlobalState::GetInstance()->GetNamesManager();
  //    namesManager.SetActorName(this, dataRepr, info, nullptr, nullptr);
  //  }
  //}

  this->Superclass::Update();
}

void vtkPVOmniConnectRenderView::Render(bool interactive, bool skip_rendering)
{
  if (!IsClientOnly)
  {
    vtkProcessModule* pm = vtkProcessModule::GetProcessModule();
    vtkPVSessionBase* activeSession = vtkPVSessionBase::SafeDownCast(pm->GetActiveSession());
    vtkPVSessionCore* sessionCore = activeSession->GetSessionCore();

    vtkOmniConnectRendererNode* rendererNode = OmniConnectPass->GetSceneGraph();
    vtkMultiProcessController* controller = vtkMultiProcessController::GetGlobalController();
    if (rendererNode)
      rendererNode->SetProgressNotifier(nullptr);

    ReceiveAnimatedObjects();
    UpdateActorNamesFromProxy();

    int num_reprs = this->GetNumberOfRepresentations();
    for (int i = 0; i < num_reprs; ++i)
    {
      vtkAlgorithm* repr = this->GetRepresentation(i);

      // Allow handling progress on this representation
      if (repr != nullptr)
      {
        if (rendererNode && controller->GetNumberOfProcesses() == 1)
          rendererNode->SetProgressNotifier(repr);
      }

      // Retrieve representation (for passing on name/time)
      vtkProp3D* actor = nullptr;
      vtkPVDataRepresentation* dataRepr = vtkPVDataRepresentation::SafeDownCast(repr);
      if (dataRepr)
      {
        actor = vtkPVOmniConnectNamesManager::GetActorFromRepresentation(dataRepr);
        if (!actor)
          continue;
      }
      else
        continue;
      
      // Set name/time properties
      vtkInformation* actInfo = vtkPVOmniConnectNamesManager::GetOrCreatePropertyKeys(actor);

      // Specialized source property setting for glyph mappers
      AddGlyphSourcePropertyKeys(dataRepr, actInfo);

      bool hasAnimParent = HasAnimatedParent(dataRepr->GetExecutive());
      vtkDataObject* inputDataObj = dataRepr->GetInputDataObject(0, 0);

      if (inputDataObj)
      {
        vtkInformation* inputInfo = inputDataObj->GetInformation();
        double dataTime = 0.0;

        if(hasAnimParent)
          dataTime = ViewTime; // If a parent is animated, every timestep is considered to be different (ie. based on viewtime), regardless of DATA_TIME_STEP
        else if(inputInfo->Has(vtkDataObject::DATA_TIME_STEP()))
          dataTime = inputInfo->Get(vtkDataObject::DATA_TIME_STEP());

        actInfo->Set(vtkDataObject::DATA_TIME_STEP(), dataTime*GlobalTimeScale);
      }

      vtkPVOmniConnectNamesManager& namesManager = vtkPVOmniConnectGlobalState::GetInstance()->GetNamesManager();
      namesManager.SetActorName(this, dataRepr, actInfo, nullptr, sessionCore, false);
    }
  }

  this->Superclass::Render(interactive, true); // Workaround: Deal with update requests first, which often leaves mappers in a dirty state. No rendering.
  this->Superclass::Render(interactive, skip_rendering); // Update again to deal with the dirty state. Then render.
}

void vtkPVOmniConnectRenderView::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

void vtkPVOmniConnectRenderView::SetViewTime(double value)
{
  ViewTime = value;

  if(OmniConnectPass->GetSceneGraph())
    OmniConnectPass->GetSceneGraph()->SetSceneTime(value*GlobalTimeScale);

  this->Superclass::SetViewTime(value);
}

void vtkPVOmniConnectRenderView::SetRemoteRenderingThreshold(double threshold)
{
  this->Superclass::SetRemoteRenderingThreshold(0.0);
}

void vtkPVOmniConnectRenderView::SetLODRenderingThreshold(double threshold)
{
  this->Superclass::SetLODRenderingThreshold(1.0e+11);
}

void vtkPVOmniConnectRenderView::SetRenderingEnabled(bool enabled)
{
  RenderingEnabled = enabled;

  vtkOmniConnectRendererNode* rendererNode = OmniConnectPass->GetSceneGraph();
  vtkRenderWindow* win = this->GetRenderWindow();
  if(win && rendererNode)
  {
    vtkObject* omniRenderer = rendererNode->GetRenderable();

    vtkRendererCollection* rens = win->GetRenderers();
    vtkCollectionSimpleIterator rsit;
    vtkRenderer* ren = nullptr;
    for (rens->InitTraversal(rsit); (ren = rens->GetNextRenderer(rsit));)
    {
      if (ren != omniRenderer)
        ren->SetDraw(enabled);
    }
  }

  if (OmniConnectPass->GetSceneGraph())
    OmniConnectPass->GetSceneGraph()->SetRenderingEnabled(enabled);

  this->Modified();
}

void vtkPVOmniConnectRenderView::SetForceTimeVaryingTextures(bool enabled)
{
  ForceTimeVaryingTextures = enabled;

  if (OmniConnectPass->GetSceneGraph())
    OmniConnectPass->GetSceneGraph()->SetForceTimeVaryingTextures(enabled);
  else if (enabled && !IsClientOnly)
    vtkOmniConnectLogCallback::Callback(OmniConnectLogLevel::ERR, nullptr, "'Force timevarying textures' cannot be enabled: view initialization has failed.");

  this->Modified();
}
void vtkPVOmniConnectRenderView::SetForceTimeVaryingMaterialParams(bool enabled)
{
  ForceTimeVaryingMaterialParams = enabled;

  if (OmniConnectPass->GetSceneGraph())
    OmniConnectPass->GetSceneGraph()->SetForceTimeVaryingMaterialParams(enabled);
  else if (enabled && !IsClientOnly)
    vtkOmniConnectLogCallback::Callback(OmniConnectLogLevel::ERR, nullptr, "'Force timevarying parameters' cannot be enabled: view initialization has failed.");

  this->Modified();
}

void vtkPVOmniConnectRenderView::SetForceConsistentWinding(bool enabled)
{
  ForceConsistentWinding = enabled;

  if (OmniConnectPass->GetSceneGraph())
    OmniConnectPass->GetSceneGraph()->SetForceConsistentWinding(enabled);
  else if (enabled && !IsClientOnly)
    vtkOmniConnectLogCallback::Callback(OmniConnectLogLevel::ERR, nullptr, "'Force consistent winding' cannot be enabled: view initialization has failed.");

  this->Modified();
}

void vtkPVOmniConnectRenderView::SetMergeTfIntoVol(bool enabled)
{
  MergeTfIntoVol = enabled;

  if (OmniConnectPass->GetSceneGraph())
    OmniConnectPass->GetSceneGraph()->SetMergeTfIntoVol(enabled);
  else if (enabled && !IsClientOnly)
    vtkOmniConnectLogCallback::Callback(OmniConnectLogLevel::ERR, nullptr, "'Preclassified volumes' cannot be enabled: view initialization has failed.");

  this->Modified();
}

void vtkPVOmniConnectRenderView::SetAllowWidgetActors(bool enabled)
{
  AllowWidgetActors = enabled;

  if (OmniConnectPass->GetSceneGraph())
    OmniConnectPass->GetSceneGraph()->SetAllowWidgetActors(enabled);
  else if(enabled && !IsClientOnly)
    vtkOmniConnectLogCallback::Callback(OmniConnectLogLevel::ERR, nullptr, "'Allow Widget Actors' cannot be enabled: view initialization has failed.");

  this->Modified();
}

void vtkPVOmniConnectRenderView::SetLiveWorkflowEnabled(bool enabled)
{
  LiveWorkflowEnabled = enabled;

  if (OmniConnectPass->GetSceneGraph())
    OmniConnectPass->GetSceneGraph()->SetLiveWorkflowEnabled(enabled);
  else if(enabled && !IsClientOnly)
    vtkOmniConnectLogCallback::Callback(OmniConnectLogLevel::ERR, nullptr, "'Live workflow' cannot be enabled: view initialization has failed.");

  this->Modified();
}

void vtkPVOmniConnectRenderView::SetGlobalTimeScale(double globalTimeScale)
{
  GlobalTimeScale = globalTimeScale;

  this->Modified();
}



