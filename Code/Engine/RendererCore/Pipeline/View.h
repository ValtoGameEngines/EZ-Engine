#pragma once

#include <Foundation/Profiling/Profiling.h>
#include <Foundation/Strings/HashedString.h>
#include <Foundation/Threading/DelegateTask.h>
#include <Foundation/Types/TagSet.h>
#include <Foundation/Types/UniquePtr.h>

#include <CoreUtils/NodeGraph/Node.h>
#include <RendererFoundation/Resources/RenderTargetSetup.h>
#include <RendererCore/Pipeline/ViewData.h>
#include <RendererCore/Pipeline/RenderPipelineResource.h>

class ezWorld;
class ezRenderPipeline;

/// \brief Encapsulates a view on the given world through the given camera
/// and rendered with the specified RenderPipeline into the given render target setup.
class EZ_RENDERERCORE_DLL ezView : public ezNode
{
  EZ_ADD_DYNAMIC_REFLECTION(ezView, ezNode);

private:
  /// \brief Use ezRenderLoop::CreateView to create a view.
  ezView();
  ~ezView();

public:
  void SetName(const char* szName);
  const char* GetName() const;
  
  void SetWorld(ezWorld* pWorld);
  ezWorld* GetWorld();
  const ezWorld* GetWorld() const;

  
  void SetRenderTargetSetup(ezGALRenderTagetSetup& renderTargetSetup);
  const ezGALRenderTagetSetup& GetRenderTargetSetup() const;

  void SetRenderPipelineResource(ezRenderPipelineResourceHandle hPipeline);
  ezRenderPipelineResourceHandle GetRenderPipelineResource() const;

  ezRenderPipeline* GetRenderPipeline() const;

  void SetLogicCamera(const ezCamera* pCamera);
  const ezCamera* GetLogicCamera() const;

  void SetRenderCamera(const ezCamera* pCamera);
  const ezCamera* GetRenderCamera() const;

  void SetViewport(const ezRectFloat& viewport);
  const ezRectFloat& GetViewport() const;

  const ezViewData& GetData() const;

  bool IsValid() const;

  /// \brief Rebuilds pipeline if necessary and pushes double-buffered settings into the pipeline.
  void EnsureUpToDate();

  void ReadBackPassProperties();

  /// \brief Extracts all relevant data from the world to render the view.
  void ExtractData();

  /// \brief Returns a task implementation that calls ExtractData on this view.
  ezTask* GetExtractTask();


  /// \brief Returns the start position and direction (in world space) of the picking ray through the screen position in this view.
  ///
  /// fScreenPosX and fScreenPosY are expected to be in [0; 1] range (normalized pixel coordinates).
  /// If no ray can be computed, EZ_FAILURE is returned.
  ezResult ComputePickingRay(float fScreenPosX, float fScreenPosY, ezVec3& out_RayStartPos, ezVec3& out_RayDir);
  
  /// \brief Returns the current projection matrix.
  const ezMat4& GetProjectionMatrix() const;

  /// \brief Returns the current inverse projection matrix.
  const ezMat4& GetInverseProjectionMatrix() const;

  /// \brief Returns the current view matrix (camera orientation).
  const ezMat4& GetViewMatrix() const;

  /// \brief Returns the current inverse view matrix (inverse camera orientation).
  const ezMat4& GetInverseViewMatrix() const;

  /// \brief Returns the current view-projection matrix.
  const ezMat4& GetViewProjectionMatrix() const;

  /// \brief Returns the current inverse view-projection matrix.
  const ezMat4& GetInverseViewProjectionMatrix() const;

  void SetRenderPassProperty(const char* szPassName, const char* szPropertyName, const ezVariant& value);
  void SetExtractorProperty(const char* szPassName, const char* szPropertyName, const ezVariant& value);

  void SetRenderPassReadBackProperty(const char* szPassName, const char* szPropertyName, const ezVariant& value);
  ezVariant GetRenderPassReadBackProperty(const char* szPassName, const char* szPropertyName);
  bool IsRenderPassReadBackPropertyExisting(const char* szPassName, const char* szPropertyName) const;

  ezTagSet m_IncludeTags;
  ezTagSet m_ExcludeTags;

private:
  friend class ezRenderLoop;

  ezHashedString m_sName;

  ezProfilingId m_ExtractDataProfilingID;

  ezDelegateTask<void> m_ExtractTask;

  ezWorld* m_pWorld;

  ezGALRenderTagetSetup m_RenderTargetSetup;
  ezRenderPipelineResourceHandle m_hRenderPipeline;
  ezUInt32 m_uiRenderPipelineResourceDescriptionCounter;
  ezUniquePtr<ezRenderPipeline> m_pRenderPipeline;
  const ezCamera* m_pLogicCamera;
  const ezCamera* m_pRenderCamera;

private:
  ezInputNodePin m_PinRenderTarget0;
  ezInputNodePin m_PinRenderTarget1;
  ezInputNodePin m_PinRenderTarget2;
  ezInputNodePin m_PinRenderTarget3;
  ezInputNodePin m_PinDepthStencil;

private:
  void UpdateCachedMatrices() const;
  void UpdateRenderPipeline(ezUniquePtr<ezRenderPipeline>&& pRenderPipeline);

  mutable ezUInt32 m_uiLastCameraSettingsModification;
  mutable ezUInt32 m_uiLastCameraOrientationModification;
  mutable float m_fLastViewportAspectRatio;
  
  mutable ezViewData m_Data;

  struct PropertyValue
  {
    ezString m_sObjectName;
    ezString m_sPropertyName;
    ezVariant m_Value;
    bool m_bIsValid;
    bool m_bIsDirty;
  };

  void SetProperty(ezMap<ezString, PropertyValue>& map, const char* szPassName, const char* szPropertyName, const ezVariant& value);
  void SetReadBackProperty(ezMap<ezString, PropertyValue>& map, const char* szPassName, const char* szPropertyName, const ezVariant& value);

  void ResetAllPropertyStates(ezMap<ezString, PropertyValue>& map);

  void ApplyRenderPassProperties();

  void ApplyProperty(ezReflectedClass* pClass, PropertyValue &data, const char* szTypeName);

  void ApplyExtractorProperties();

  ezMap<ezString, PropertyValue> m_PassProperties;
  ezMap<ezString, PropertyValue> m_PassReadBackProperties;
  ezMap<ezString, PropertyValue> m_ExtractorProperties;
};

#include <RendererCore/Pipeline/Implementation/View_inl.h>
