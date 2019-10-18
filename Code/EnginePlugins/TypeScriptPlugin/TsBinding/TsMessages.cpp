#include <TypeScriptPluginPCH.h>

#include <Duktape/duktape.h>
#include <Foundation/IO/FileSystem/FileReader.h>
#include <Foundation/IO/FileSystem/FileWriter.h>
#include <Foundation/Reflection/ReflectionUtils.h>
#include <TypeScriptPlugin/TsBinding/TsBinding.h>

void ezTypeScriptBinding::GenerateMessagesFile(const char* szFile)
{
  ezStringBuilder sFileContent;

  sFileContent =
    R"(// AUTO-GENERATED FILE

import __Message = require("./Message")
export import Message = __Message.Message;

import __Vec3 = require("./Vec3")
export import Vec3 = __Vec3.Vec3;

import __Quat = require("./Quat")
export import Quat = __Quat.Quat;

import __Color = require("./Color")
export import Color = __Color.Color;

import __Time = require("./Time")
export import Time = __Time.Time;

import __Angle = require("./Angle")
export import Angle = __Angle.Angle;

)";

  GenerateAllMessagesCode(sFileContent);

  ezFileWriter file;
  if (file.Open(szFile).Failed())
  {
    ezLog::Error("Failed to open file '{}'", szFile);
    return;
  }

  file.WriteBytes(sFileContent.GetData(), sFileContent.GetElementCount());
}

static void CreateMessageTypeList(ezSet<const ezRTTI*>& found, ezDynamicArray<const ezRTTI*>& sorted, const ezRTTI* pRtti)
{
  if (found.Contains(pRtti))
    return;

  if (!pRtti->IsDerivedFrom<ezMessage>())
    return;

  if (pRtti == ezGetStaticRTTI<ezMessage>())
    return;

  found.Insert(pRtti);
  CreateMessageTypeList(found, sorted, pRtti->GetParentType());

  sorted.PushBack(pRtti);
}

void ezTypeScriptBinding::GenerateAllMessagesCode(ezStringBuilder& out_Code)
{
  ezSet<const ezRTTI*> found;
  ezDynamicArray<const ezRTTI*> sorted;
  sorted.Reserve(100);

  for (auto pRtti = ezRTTI::GetFirstInstance(); pRtti != nullptr; pRtti = pRtti->GetNextInstance())
  {
    CreateMessageTypeList(found, sorted, pRtti);
  }

  for (auto pRtti : sorted)
  {
    GenerateMessageCode(out_Code, pRtti);
  }
}

void ezTypeScriptBinding::GenerateMessageCode(ezStringBuilder& out_Code, const ezRTTI* pRtti)
{
  ezStringBuilder sType, sParentType;
  GetTsName(pRtti, sType);

  GetTsName(pRtti->GetParentType(), sParentType);

  out_Code.AppendFormat("export class {0} extends {1}\n", sType, sParentType);
  out_Code.Append("{\n");
  out_Code.AppendFormat("  constructor() { super(); this.TypeNameHash = {}; }\n", pRtti->GetTypeNameHash());
  GenerateMessagePropertiesCode(out_Code, pRtti);
  out_Code.Append("}\n\n");
}

void ezTypeScriptBinding::GenerateMessagePropertiesCode(ezStringBuilder& out_Code, const ezRTTI* pRtti)
{
  ezStringBuilder sProp;
  ezStringBuilder sDefault;

  for (ezAbstractProperty* pProp : pRtti->GetProperties())
  {
    if (pProp->GetCategory() != ezPropertyCategory::Member)
      continue;

    ezAbstractMemberProperty* pMember = static_cast<ezAbstractMemberProperty*>(pProp);

    const char* szTypeName = TsType(pMember->GetSpecificType());
    if (szTypeName == nullptr)
      continue;

    const ezVariant def = ezReflectionUtils::GetDefaultValue(pMember);

    if (def.CanConvertTo<ezString>())
    {
      sDefault = def.ConvertTo<ezString>();
    }

    if (!sDefault.IsEmpty())
    {
      // TODO: make this prettier
      if (def.GetType() == ezVariantType::Color)
      {
        ezColor c = def.Get<ezColor>();
        sDefault.Format("new Color({}, {}, {}, {})", c.r, c.g, c.b, c.a);
      }

      sProp.Format("  {0}: {1} = {2};\n", pMember->GetPropertyName(), szTypeName, sDefault);
    }
    else
    {
      sProp.Format("  {0}: {1};\n", pMember->GetPropertyName(), szTypeName);
    }

    out_Code.Append(sProp.GetView());
  }
}

void ezTypeScriptBinding::InjectMessageImportExport(const char* szFile, const char* szMessageFile)
{
  ezSet<const ezRTTI*> found;
  ezDynamicArray<const ezRTTI*> sorted;
  sorted.Reserve(100);

  for (auto pRtti = ezRTTI::GetFirstInstance(); pRtti != nullptr; pRtti = pRtti->GetNextInstance())
  {
    CreateMessageTypeList(found, sorted, pRtti);
  }

  ezStringBuilder sImportExport, sTypeName;

  sImportExport.Format(R"(

// AUTO-GENERATED
import __AllMessages = require("{}")
)",
    szMessageFile);

  for (const ezRTTI* pRtti : sorted)
  {
    GetTsName(pRtti, sTypeName);
    sImportExport.AppendFormat("export import {0}  = __AllMessages.{0};\n",
      sTypeName);
  }


  ezStringBuilder sFinal;

  {
    ezFileReader fileIn;
    fileIn.Open(szFile);

    ezStringBuilder sSrc;
    sSrc.ReadAll(fileIn);

    //if (const char* szAutoGen = sSrc.FindSubString("// AUTO-GENERATED"))
    //{
    //  sFinal.SetSubString_FromTo(sSrc.GetData(), szAutoGen);
    //  sFinal.Trim(" \t\n\r");
    //}
    //else
    {
      sFinal = sSrc;
      sFinal.Append("\n\n");
    }

    sFinal.Append(sImportExport.GetView());
    sFinal.Append("\n");
  }

  {
    ezFileWriter fileOut;
    fileOut.Open(szFile);
    fileOut.WriteBytes(sFinal.GetData(), sFinal.GetElementCount());
  }
}

static ezUniquePtr<ezMessage> CreateMessage(ezUInt32 uiTypeHash, const ezRTTI*& pRtti)
{
  static ezHashTable<ezUInt32, const ezRTTI*, ezHashHelper<ezUInt32>, ezStaticAllocatorWrapper> MessageTypes;

  if (!MessageTypes.TryGetValue(uiTypeHash, pRtti))
  {
    for (pRtti = ezRTTI::GetFirstInstance(); pRtti != nullptr; pRtti = pRtti->GetNextInstance())
    {
      if (pRtti->GetTypeNameHash() == uiTypeHash)
      {
        MessageTypes[uiTypeHash] = pRtti;
        break;
      }
    }
  }

  if (pRtti == nullptr || !pRtti->GetAllocator()->CanAllocate())
    return nullptr;

  return pRtti->GetAllocator()->Allocate<ezMessage>();
}

ezUniquePtr<ezMessage> ezTypeScriptBinding::MessageFromParameter(duk_context* pDuk, ezInt32 iObjIdx)
{
  ezDuktapeHelper duk(pDuk, 0);

  ezUInt32 uiTypeNameHash = duk.GetUIntValue(iObjIdx);

  const ezRTTI* pRtti = nullptr;
  ezUniquePtr<ezMessage> pMsg = CreateMessage(uiTypeNameHash, pRtti);


  ezHybridArray<ezAbstractProperty*, 32> properties;
  pRtti->GetAllProperties(properties);

  for (ezAbstractProperty* pProp : properties)
  {
    if (pProp->GetCategory() != ezPropertyCategory::Member)
      continue;

    ezAbstractMemberProperty* pMember = static_cast<ezAbstractMemberProperty*>(pProp);

    const ezVariant::Type::Enum type = pMember->GetSpecificType()->GetVariantType();
    switch (type)
    {
      case ezVariant::Type::Invalid:
        break;

      case ezVariant::Type::Bool:
      {
        bool value = duk.GetBoolProperty(pMember->GetPropertyName(), false, iObjIdx + 1);
        ezReflectionUtils::SetMemberPropertyValue(pMember, pMsg.Borrow(), value);
        break;
      }

      case ezVariant::Type::String:
      case ezVariant::Type::StringView:
      {
        ezStringView value = duk.GetStringProperty(pMember->GetPropertyName(), "", iObjIdx + 1);
        ezReflectionUtils::SetMemberPropertyValue(pMember, pMsg.Borrow(), value);
        break;
      }

      case ezVariant::Type::Int8:
      case ezVariant::Type::Int16:
      case ezVariant::Type::Int32:
      case ezVariant::Type::Int64:
      {
        ezInt32 value = duk.GetIntProperty(pMember->GetPropertyName(), 0, iObjIdx + 1);
        ezReflectionUtils::SetMemberPropertyValue(pMember, pMsg.Borrow(), value);
        break;
      }

      case ezVariant::Type::UInt8:
      case ezVariant::Type::UInt16:
      case ezVariant::Type::UInt32:
      case ezVariant::Type::UInt64:
      {
        ezUInt32 value = duk.GetUIntProperty(pMember->GetPropertyName(), 0, iObjIdx + 1);
        ezReflectionUtils::SetMemberPropertyValue(pMember, pMsg.Borrow(), value);
        break;
      }

      case ezVariant::Type::Float:
      {
        const float value = duk.GetFloatProperty(pMember->GetPropertyName(), 0, iObjIdx + 1);
        ezReflectionUtils::SetMemberPropertyValue(pMember, pMsg.Borrow(), value);
        break;
      }

      case ezVariant::Type::Double:
      {
        const double value = duk.GetNumberProperty(pMember->GetPropertyName(), 0, iObjIdx + 1);
        ezReflectionUtils::SetMemberPropertyValue(pMember, pMsg.Borrow(), value);
        break;
      }

      case ezVariant::Type::Vector3:
      {
        ezVec3 value = ezTypeScriptBinding::GetVec3Property(duk, pMember->GetPropertyName(), iObjIdx + 1);
        ezReflectionUtils::SetMemberPropertyValue(pMember, pMsg.Borrow(), value);
        break;
      }

      case ezVariant::Type::Quaternion:
      {
        ezQuat value = ezTypeScriptBinding::GetQuatProperty(duk, pMember->GetPropertyName(), iObjIdx + 1);
        ezReflectionUtils::SetMemberPropertyValue(pMember, pMsg.Borrow(), value);
        break;
      }

      case ezVariant::Type::Color:
      {
        ezColor value = ezTypeScriptBinding::GetColorProperty(duk, pMember->GetPropertyName(), iObjIdx + 1);
        ezReflectionUtils::SetMemberPropertyValue(pMember, pMsg.Borrow(), value);
        break;
      }

      case ezVariant::Type::ColorGamma:
      {
        ezColorGammaUB value = ezTypeScriptBinding::GetColorProperty(duk, pMember->GetPropertyName(), iObjIdx + 1);
        ezReflectionUtils::SetMemberPropertyValue(pMember, pMsg.Borrow(), value);
        break;
      }

      case ezVariant::Type::Time:
      {
        const ezTime value = ezTime::Seconds(duk.GetNumberProperty(pMember->GetPropertyName(), 0, iObjIdx + 1));
        ezReflectionUtils::SetMemberPropertyValue(pMember, pMsg.Borrow(), value);
        break;
      }

      case ezVariant::Type::Angle:
      {
        const ezAngle value = ezAngle::Radian(duk.GetFloatProperty(pMember->GetPropertyName(), 0, iObjIdx + 1));
        ezReflectionUtils::SetMemberPropertyValue(pMember, pMsg.Borrow(), value);
        break;
      }

      case ezVariant::Type::Vector2:
      case ezVariant::Type::Matrix3:
      case ezVariant::Type::Matrix4:
      case ezVariant::Type::Uuid:
      default:
        EZ_ASSERT_NOT_IMPLEMENTED;
    }
  }

  return pMsg;
}
