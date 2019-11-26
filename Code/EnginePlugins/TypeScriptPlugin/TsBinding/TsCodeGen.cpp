#include <TypeScriptPluginPCH.h>

#include <Foundation/IO/FileSystem/FileReader.h>
#include <Foundation/IO/FileSystem/FileWriter.h>
#include <TypeScriptPlugin/TsBinding/TsBinding.h>

void ezTypeScriptBinding::RemoveAutoGeneratedCode(const char* szFile)
{
  ezStringBuilder sFinal;

  {
    ezFileReader fileIn;
    fileIn.Open(szFile);

    ezStringBuilder sSrc;
    sSrc.ReadAll(fileIn);

    if (const char* szAutoGen = sSrc.FindSubString("// AUTO-GENERATED"))
    {
      sFinal.SetSubString_FromTo(sSrc.GetData(), szAutoGen);
    }
    else
    {
      sFinal = sSrc;
    }

    sFinal.Trim(" \t\n\r");
    sFinal.Append("\n\n");
  }

  {
    ezFileWriter fileOut;
    fileOut.Open(szFile);
    fileOut.WriteBytes(sFinal.GetData(), sFinal.GetElementCount());
  }
}

void ezTypeScriptBinding::AppendToTextFile(const char* szFile, ezStringView text)
{
  ezStringBuilder sFinal;

  {
    ezFileReader fileIn;
    fileIn.Open(szFile);

    sFinal.ReadAll(fileIn);

    sFinal.Append("\n\n// AUTO-GENERATED\n");
    sFinal.Append(text);
    sFinal.Append("\n");
  }

  {
    ezFileWriter fileOut;
    fileOut.Open(szFile);
    fileOut.WriteBytes(sFinal.GetData(), sFinal.GetElementCount());
  }
}

void ezTypeScriptBinding::GenerateEnumsFile()
{
  const char* szFile = ":project/TypeScript/ez/AllEnums.ts";

  ezStringBuilder sFileContent;
  ezStringBuilder sType, sName;

  sFileContent = "// AUTO-GENERATED FILE\n\n";

  for (const ezRTTI* pRtti : s_RequiredEnums)
  {
    if (pRtti->GetProperties().IsEmpty())
      continue;

    if (pRtti->IsDerivedFrom<ezEnumBase>() || pRtti->IsDerivedFrom<ezBitflagsBase>())
    {
      sName = pRtti->GetTypeName();
      sName.TrimWordStart("ez");

      sType.Format("export enum {0} { ", sName);

      for (auto pProp : pRtti->GetProperties().GetSubArray(1))
      {
        if (pProp->GetCategory() == ezPropertyCategory::Constant)
        {
          const ezVariant value = static_cast<const ezAbstractConstantProperty*>(pProp)->GetConstant();
          const ezInt64 iValue = value.ConvertTo<ezInt64>();

          sType.AppendFormat(" {0} = {1},", ezStringUtils::FindLastSubString(pProp->GetPropertyName(), "::") + 2, iValue);
        }
      }

      sType.Shrink(0, 1);
      sType.Append(" }\n");

      sFileContent.Append(sType.GetView());
    }
  }

  ezFileWriter file;
  if (file.Open(szFile).Failed())
  {
    ezLog::Error("Failed to open file '{}'", szFile);
    return;
  }

  file.WriteBytes(sFileContent.GetData(), sFileContent.GetElementCount());
}

void ezTypeScriptBinding::InjectEnumImportExport(const char* szFile, const char* szEnumFile)
{
  ezStringBuilder sImportExport, sTypeName;

  sImportExport.Format("import __AllEnums = require(\"{}\")\n", szEnumFile);

  for (const ezRTTI* pRtti : s_RequiredEnums)
  {
    GetTsName(pRtti, sTypeName);
    sImportExport.AppendFormat("export import {0} = __AllEnums.{0};\n", sTypeName);
  }

  AppendToTextFile(szFile, sImportExport);
}