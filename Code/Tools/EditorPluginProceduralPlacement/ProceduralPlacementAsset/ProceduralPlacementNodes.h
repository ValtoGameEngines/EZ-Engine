#pragma once

#include <Foundation/Utilities/Node.h>
#include <ProceduralPlacementPlugin/VM/ExpressionAST.h>

class ezProceduralPlacementNodeBase : public ezReflectedClass
{
  EZ_ADD_DYNAMIC_REFLECTION(ezProceduralPlacementNodeBase, ezReflectedClass);

public:
  virtual ezExpressionAST::Node* GenerateExpressionASTNode(ezArrayPtr<ezExpressionAST::Node*> inputs, ezExpressionAST& out_Ast) = 0;
};

//////////////////////////////////////////////////////////////////////////

class ezProceduralPlacementLayerOutput : public ezProceduralPlacementNodeBase
{
  EZ_ADD_DYNAMIC_REFLECTION(ezProceduralPlacementLayerOutput, ezProceduralPlacementNodeBase);

public:
  virtual ezExpressionAST::Node* GenerateExpressionASTNode(ezArrayPtr<ezExpressionAST::Node*> inputs, ezExpressionAST& out_Ast) override;

  void Save(ezStreamWriter& stream);

  bool m_bActive = true;

  ezString m_sName;
  ezHybridArray<ezString, 4> m_ObjectsToPlace;

  float m_fFootprint = 1.0f;

  ezVec3 m_vMinOffset = ezVec3(0);
  ezVec3 m_vMaxOffset = ezVec3(0);

  float m_fAlignToNormal = 1.0f;

  ezVec3 m_vMinScale = ezVec3(1);
  ezVec3 m_vMaxScale = ezVec3(1);

  float m_fCullDistance = 30.0f;

  ezUInt32 m_uiCollisionLayer = 0;

  ezString m_sSurface;

  ezString m_sColorGradient;

  ezUInt32 m_uiByteCodeIndex = ezInvalidIndex;

  ezInputNodePin m_DensityPin;
  ezInputNodePin m_ScalePin;
  ezInputNodePin m_ColorIndexPin;
  ezInputNodePin m_ObjectIndexPin;
};

//////////////////////////////////////////////////////////////////////////

class ezProceduralPlacementRandom : public ezProceduralPlacementNodeBase
{
  EZ_ADD_DYNAMIC_REFLECTION(ezProceduralPlacementRandom, ezProceduralPlacementNodeBase);

public:
  virtual ezExpressionAST::Node* GenerateExpressionASTNode(ezArrayPtr<ezExpressionAST::Node*> inputs, ezExpressionAST& out_Ast) override;

  ezInt32 m_iSeed = -1;

  ezOutputNodePin m_OutputValuePin;

private:
  void OnObjectCreated(const ezAbstractObjectNode& node);

  ezUInt32 m_uiAutoSeed;
};

//////////////////////////////////////////////////////////////////////////

class ezProceduralPlacementBlend : public ezProceduralPlacementNodeBase
{
  EZ_ADD_DYNAMIC_REFLECTION(ezProceduralPlacementBlend, ezProceduralPlacementNodeBase);

public:
  virtual ezExpressionAST::Node* GenerateExpressionASTNode(ezArrayPtr<ezExpressionAST::Node*> inputs, ezExpressionAST& out_Ast) override;

  float m_fInputValueA = 1.0f;
  float m_fInputValueB = 1.0f;

  ezInputNodePin m_InputValueAPin;
  ezInputNodePin m_InputValueBPin;
  ezOutputNodePin m_OutputValuePin;
};

//////////////////////////////////////////////////////////////////////////

class ezProceduralPlacementHeight : public ezProceduralPlacementNodeBase
{
  EZ_ADD_DYNAMIC_REFLECTION(ezProceduralPlacementHeight, ezProceduralPlacementNodeBase);

public:
  virtual ezExpressionAST::Node* GenerateExpressionASTNode(ezArrayPtr<ezExpressionAST::Node*> inputs, ezExpressionAST& out_Ast) override;

  float m_fMinHeight = 0.0f;
  float m_fMaxHeight = 1000.0f;
  float m_fFadeFraction = 0.2f;

  ezOutputNodePin m_OutputValuePin;
};


