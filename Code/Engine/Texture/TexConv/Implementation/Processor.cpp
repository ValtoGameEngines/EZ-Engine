#include <PCH.h>

#include <Texture/Image/ImageUtils.h>
#include <Texture/TexConv/TexConvProcessor.h>

// clang=format off
EZ_BEGIN_STATIC_REFLECTED_ENUM(ezTexConvCompressionMode, 1)
  EZ_ENUM_CONSTANTS(ezTexConvCompressionMode::None, ezTexConvCompressionMode::Medium, ezTexConvCompressionMode::High)
EZ_END_STATIC_REFLECTED_ENUM;

EZ_BEGIN_STATIC_REFLECTED_ENUM(ezTexConvMipmapMode, 1)
  EZ_ENUM_CONSTANTS(ezTexConvMipmapMode::None, ezTexConvMipmapMode::Linear, ezTexConvMipmapMode::Kaiser)
EZ_END_STATIC_REFLECTED_ENUM;

EZ_BEGIN_STATIC_REFLECTED_ENUM(ezTexConvWrapMode, 1)
  EZ_ENUM_CONSTANTS(ezTexConvWrapMode::Repeat, ezTexConvWrapMode::Mirror, ezTexConvWrapMode::Clamp)
EZ_END_STATIC_REFLECTED_ENUM;

EZ_BEGIN_STATIC_REFLECTED_ENUM(ezTexConvFilterMode, 1)
  EZ_ENUM_CONSTANT(ezTexConvFilterMode::FixedNearest), EZ_ENUM_CONSTANT(ezTexConvFilterMode::FixedBilinear),
    EZ_ENUM_CONSTANT(ezTexConvFilterMode::FixedTrilinear), EZ_ENUM_CONSTANT(ezTexConvFilterMode::FixedAnisotropic2x),
    EZ_ENUM_CONSTANT(ezTexConvFilterMode::FixedAnisotropic4x), EZ_ENUM_CONSTANT(ezTexConvFilterMode::FixedAnisotropic8x),
    EZ_ENUM_CONSTANT(ezTexConvFilterMode::FixedAnisotropic16x), EZ_ENUM_CONSTANT(ezTexConvFilterMode::LowestQuality),
    EZ_ENUM_CONSTANT(ezTexConvFilterMode::LowQuality), EZ_ENUM_CONSTANT(ezTexConvFilterMode::DefaultQuality),
    EZ_ENUM_CONSTANT(ezTexConvFilterMode::HighQuality), EZ_ENUM_CONSTANT(ezTexConvFilterMode::HighestQuality),
EZ_END_STATIC_REFLECTED_ENUM;
// clang=format on

ezTexConvProcessor::ezTexConvProcessor() = default;

ezResult ezTexConvProcessor::Process()
{
  if (m_Descriptor.m_OutputType == ezTexConvOutputType::DecalAtlas)
  {
    ezMemoryStreamWriter stream(&m_DecalAtlas);
    EZ_SUCCEED_OR_RETURN(GenerateDecalAtlas(stream));
  }
  else
  {
    ezUInt32 uiNumChannelsUsed = 0;
    EZ_SUCCEED_OR_RETURN(DetectNumChannels(m_Descriptor.m_ChannelMappings, uiNumChannelsUsed));

    EZ_SUCCEED_OR_RETURN(LoadInputImages());

    EZ_SUCCEED_OR_RETURN(AdjustUsage(m_Descriptor.m_InputFiles[0], m_Descriptor.m_InputImages[0], m_Descriptor.m_Usage));

    EZ_SUCCEED_OR_RETURN(ForceSRGBFormats());

    ezEnum<ezImageFormat> OutputImageFormat;

    EZ_SUCCEED_OR_RETURN(ChooseOutputFormat(OutputImageFormat, m_Descriptor.m_Usage, uiNumChannelsUsed));

    ezUInt32 uiTargetResolutionX = 0;
    ezUInt32 uiTargetResolutionY = 0;

    EZ_SUCCEED_OR_RETURN(
      DetermineTargetResolution(m_Descriptor.m_InputImages[0], OutputImageFormat, uiTargetResolutionX, uiTargetResolutionY));

    EZ_SUCCEED_OR_RETURN(ConvertAndScaleInputImages(uiTargetResolutionX, uiTargetResolutionY));

    ezImage img2D;
    EZ_SUCCEED_OR_RETURN(Assemble2DTexture(m_Descriptor.m_InputImages[0].GetHeader(), img2D));

    EZ_SUCCEED_OR_RETURN(AdjustHdrExposure(img2D));

    EZ_SUCCEED_OR_RETURN(GenerateMipmaps(img2D));

    EZ_SUCCEED_OR_RETURN(PremultiplyAlpha(img2D));

    EZ_SUCCEED_OR_RETURN(GenerateOutput(std::move(img2D), m_OutputImage, OutputImageFormat));

    EZ_SUCCEED_OR_RETURN(GenerateThumbnailOutput(m_OutputImage, m_ThumbnailOutputImage, m_Descriptor.m_uiThumbnailOutputResolution));

    EZ_SUCCEED_OR_RETURN(GenerateLowResOutput(m_OutputImage, m_LowResOutputImage, m_Descriptor.m_uiLowResMipmaps));
  }

  return EZ_SUCCESS;
}

ezResult ezTexConvProcessor::DetectNumChannels(ezArrayPtr<const ezTexConvSliceChannelMapping> channelMapping, ezUInt32& uiNumChannels)
{
  uiNumChannels = 0;

  for (const auto& mapping : channelMapping)
  {
    for (ezUInt32 i = 0; i < 4; ++i)
    {
      if (mapping.m_Channel[i].m_iInputImageIndex != -1)
      {
        uiNumChannels = ezMath::Max(uiNumChannels, i + 1);
      }
    }
  }

  if (uiNumChannels == 0)
  {
    ezLog::Error("No proper channel mapping provided.");
    return EZ_FAILURE;
  }

  return EZ_SUCCESS;
}

ezResult ezTexConvProcessor::GenerateOutput(ezImage&& src, ezImage& dst, ezEnum<ezImageFormat> format)
{
  dst.ResetAndMove(std::move(src));

  if (dst.Convert(format).Failed())
  {
    ezLog::Error("Failed to convert result image to output format '{}'", ezImageFormat::GetName(format));
    return EZ_FAILURE;
  }

  return EZ_SUCCESS;
}

ezResult ezTexConvProcessor::GenerateThumbnailOutput(const ezImage& srcImg, ezImage& dstImg, ezUInt32 uiTargetRes)
{
  if (uiTargetRes == 0)
    return EZ_SUCCESS;

  ezUInt32 uiBestMip = 0;

  for (ezUInt32 m = 0; m < srcImg.GetNumMipLevels(); ++m)
  {
    if (srcImg.GetWidth(m) <= uiTargetRes && srcImg.GetHeight(m) <= uiTargetRes)
    {
      uiBestMip = m;
      break;
    }

    uiBestMip = m;
  }

  ezImage scratch1, scratch2;
  ezImage* pCurrentScratch = &scratch1;
  ezImage* pOtherScratch = &scratch2;

  pCurrentScratch->ResetAndCopy(srcImg.GetSubImageView(uiBestMip));

  if (pCurrentScratch->GetWidth() > uiTargetRes || pCurrentScratch->GetHeight() > uiTargetRes)
  {
    if (pCurrentScratch->GetWidth() > pCurrentScratch->GetHeight())
    {
      const float fAspectRatio = (float)pCurrentScratch->GetWidth() / (float)uiTargetRes;
      ezUInt32 uiTargetHeight = (ezUInt32)(pCurrentScratch->GetHeight() / fAspectRatio);

      uiTargetHeight = ezMath::Max(uiTargetHeight, 4U);

      if (ezImageUtils::Scale(*pCurrentScratch, *pOtherScratch, uiTargetRes, uiTargetHeight).Failed())
      {
        ezLog::Error("Failed to resize thumbnail image from {}x{} to {}x{}", pCurrentScratch->GetWidth(),
          pCurrentScratch->GetHeight(), uiTargetRes, uiTargetHeight);
        return EZ_FAILURE;
      }
    }
    else
    {
      const float fAspectRatio = (float)pCurrentScratch->GetHeight() / (float)uiTargetRes;
      ezUInt32 uiTargetWidth = (ezUInt32)(pCurrentScratch->GetWidth() / fAspectRatio);

      uiTargetWidth = ezMath::Max(uiTargetWidth, 4U);

      if (ezImageUtils::Scale(*pCurrentScratch, *pOtherScratch, uiTargetWidth, uiTargetRes).Failed())
      {
        ezLog::Error("Failed to resize thumbnail image from {}x{} to {}x{}", pCurrentScratch->GetWidth(),
          pCurrentScratch->GetHeight(), uiTargetWidth, uiTargetRes);
        return EZ_FAILURE;
      }
    }

    ezMath::Swap(pCurrentScratch, pOtherScratch);
  }

  dstImg.ResetAndMove(std::move(*pCurrentScratch));
  dstImg.ReinterpretAs(ezImageFormat::AsLinear(dstImg.GetImageFormat()));

  if (dstImg.Convert(ezImageFormat::R8G8B8A8_UNORM).Failed())
  {
    ezLog::Error("Failed to convert thumbnail image to RGBA8.");
    return EZ_FAILURE;
  }

  return EZ_SUCCESS;
}

ezResult ezTexConvProcessor::GenerateLowResOutput(const ezImage& srcImg, ezImage& dstImg, ezUInt32 uiLowResMip)
{
  if (uiLowResMip == 0)
    return EZ_SUCCESS;

  if (srcImg.GetNumMipLevels() <= uiLowResMip)
  {
    // probably just a low-resolution input image, do not generate output, but also do not fail
    ezLog::Warning("LowRes image not generated, original resolution is already below threshold.");
    return EZ_SUCCESS;
  }

  if (ezImageUtils::ExtractLowerMipChain(srcImg, dstImg, uiLowResMip).Failed())
  {
    ezLog::Error("Failed to extract low-res mipmap chain from output image.");
    return EZ_FAILURE;
  }

  return EZ_SUCCESS;
}



EZ_STATICLINK_FILE(Texture, Texture_TexConv_Implementation_Processor);
