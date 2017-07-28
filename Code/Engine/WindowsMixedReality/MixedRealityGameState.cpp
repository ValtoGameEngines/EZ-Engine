﻿#include <PCH.h>
#include <WindowsMixedReality/MixedRealityGameState.h>
#include <WindowsMixedReality/HolographicSpace.h>
#include <WindowsMixedReality/SpatialLocationService.h>
#include <WindowsMixedReality/SpatialReferenceFrame.h>
#include <WindowsMixedReality/Graphics/MixedRealityCamera.h>
#include <WindowsMixedReality/Graphics/MixedRealityDX11Device.h>
#include <WindowsMixedReality/Graphics/MixedRealitySwapChainDX11.h>
#include <WindowsMixedReality/SpatialMapping/SurfaceReconstructionMeshManager.h>

#include <GameEngine/GameApplication/GameApplication.h>
#include <GameEngine/GameState/GameStateWindow.h>

#include <RendererCore/Components/CameraComponent.h>
#include <RendererCore/RenderWorld/RenderWorld.h>
#include <RendererCore/Pipeline/View.h>

#include <Foundation/Threading/Lock.h>
#include <Core/World/World.h>

#include <windows.perception.spatial.h>
#include <Core/Input/InputManager.h>

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezMixedRealityGameState, 1, ezRTTIDefaultAllocator<ezMixedRealityGameState>);
EZ_END_DYNAMIC_REFLECTED_TYPE

ezMixedRealityGameState::ezMixedRealityGameState()
{}

ezMixedRealityGameState::~ezMixedRealityGameState()
{}

void ezMixedRealityGameState::OnActivation(ezWorld* pWorld)
{
  m_bStateWantsToQuit = false;
  m_pMainWorld = pWorld;

  m_pMainWindow = EZ_DEFAULT_NEW(ezGameStateWindow, ezWindowCreationDesc());
  GetApplication()->AddWindow(m_pMainWindow, ezGALSwapChainHandle());

  auto pHoloSpace = ezWindowsHolographicSpace::GetSingleton();

  // Holographic/Stereo!
  {
    pHoloSpace->Activate();

    pHoloSpace->m_cameraAddedEvent.AddEventHandler(ezMakeDelegate(&ezMixedRealityGameState::OnHolographicCameraAdded, this));

    // Need to handle add/remove cameras before anything else - world update won't happen without a view which may be created/destroyed by this.
    GetApplication()->m_Events.AddEventHandler([this](const ezGameApplicationEvent& evt)
    {
      if (evt.m_Type == ezGameApplicationEvent::Type::BeginFrame)
      {
        ezWindowsHolographicSpace::GetSingleton()->ProcessAddedRemovedCameras();
      }
    });
  }

  ConfigureMainCamera();

  ConfigureInputDevices();

  ConfigureInputActions();

  m_pSpatialMappingManager = EZ_DEFAULT_NEW(ezSurfaceReconstructionMeshManager);
}

void ezMixedRealityGameState::OnDeactivation()
{
  auto pHoloSpace = ezWindowsHolographicSpace::GetSingleton();

  pHoloSpace->m_cameraAddedEvent.RemoveEventHandler(ezMakeDelegate(&ezMixedRealityGameState::OnHolographicCameraAdded, this));

  pHoloSpace->SetDefaultReferenceFrame(nullptr);
}

float ezMixedRealityGameState::CanHandleThis(ezGameApplicationType AppType, ezWorld* pWorld) const
{
  if (pWorld == nullptr)
    return -1.0f;

  if (ezWindowsHolographicSpace::GetSingleton()->IsAvailable())
    return -1.0f;

  return 1.0f;
}

void ezMixedRealityGameState::SetupMainView(ezGALRenderTargetViewHandle hBackBuffer)
{
  // HololensRenderPipeline.ezRendePipelineAsset
  ezFallbackGameState::SetupMainView(hBackBuffer, ezResourceManager::LoadResource<ezRenderPipelineResource>("{ 2fe25ded-776c-7f9e-354f-e4c52a33d125 }"));
}

void ezMixedRealityGameState::ProcessInput()
{
  const char* szPressed = ezInputManager::GetPressedInputSlot(ezInputSlotFlags::None, ezInputSlotFlags::None);

  if (!ezStringUtils::IsNullOrEmpty(szPressed))
  {
    ezLog::Info("Pressed: {0}", szPressed);

    m_pSpatialMappingManager->PullCurrentSurfaces();
  }
}

void ezMixedRealityGameState::BeforeWorldUpdate()
{
  ezFallbackGameState::BeforeWorldUpdate();
}

void ezMixedRealityGameState::AfterWorldUpdate()
{
  EZ_LOCK(m_pMainWorld->GetReadMarker());

  auto pHoloSpace = ezWindowsHolographicSpace::GetSingleton();

  // Update the camera transform after world update so the owner node has its final position for this frame.
  // Setting the camera transform in ProcessInput introduces one frame delay.
  auto holoCameras = pHoloSpace->GetCameras();
  if (!holoCameras.IsEmpty())
  {
    ezWindowsMixedRealityCamera* pHoloCamera = holoCameras[0];

    auto viewport = pHoloCamera->GetViewport();
    m_MainCamera.SetStereoProjection(pHoloCamera->GetProjectionLeft(), pHoloCamera->GetProjectionRight(), viewport.width / viewport.height);

    ezMat4 mViewTransformLeft, mViewTransformRight;
    if (pHoloCamera->GetViewTransforms(*pHoloSpace->GetDefaultReferenceFrame(), mViewTransformLeft, mViewTransformRight).Succeeded())
    {
      m_MainCamera.SetViewMatrix(mViewTransformLeft, ezCameraEye::Left);
      m_MainCamera.SetViewMatrix(mViewTransformRight, ezCameraEye::Right);
    }
    else
    {
      ezLog::Error("Failed to retrieve the Holographic view transforms.");
    }

    // If there is an active camera component we update its position, but technically we don't need to!
    /*if (ezCameraComponent* pCamComp = FindActiveCameraComponent())
    {
      ezGameObject* pOwner = pCamComp->GetOwner();
      pOwner->SetGlobalPosition(m_MainCamera.GetCenterPosition());

      ezMat3 mRotation;
      mRotation.SetLookInDirectionMatrix(m_MainCamera.GetCenterDirForwards(), m_MainCamera.GetCenterDirUp());
      ezQuat rotationQuat;
      rotationQuat.SetFromMat3(mRotation);
      pOwner->SetGlobalRotation(rotationQuat);
    }*/
  }
}

void ezMixedRealityGameState::ConfigureInputActions()
{
}

void ezMixedRealityGameState::OnHolographicCameraAdded(const ezWindowsMixedRealityCamera& camera)
{
  if (m_hMainSwapChain.IsInvalidated())
  {
    // Set camera to stereo immediately so, the view is setup correctly.
    m_MainCamera.SetCameraMode(ezCameraMode::Stereo, 1.0f, 1.0f, 1000.0f);

    m_hMainSwapChain = ezGALDevice::GetDefaultDevice()->GetPrimarySwapChain();
    EZ_ASSERT_DEBUG(!m_hMainSwapChain.IsInvalidated(), "Primary swap chain is still invalid after a holographic camera has been added.");

    const ezGALSwapChain* pSwapChain = ezGALDevice::GetDefaultDevice()->GetSwapChain(m_hMainSwapChain);
    SetupMainView(ezGALDevice::GetDefaultDevice()->GetDefaultRenderTargetView(pSwapChain->GetBackBufferTexture()));

    // Viewport is different from window size (which is what SetupMainView will use), need to set manually!
    ezView* pView = nullptr;
    ezRenderWorld::TryGetView(m_hMainView, pView);
    pView->SetViewport(camera.GetViewport());

    GetApplication()->SetSwapChain(m_pMainWindow, m_hMainSwapChain);
  }
  else
  {
    ezLog::Warning("New holographic camera was added but ezMixedRealityGameState supports currently only a single holographic camera!");
  }
}