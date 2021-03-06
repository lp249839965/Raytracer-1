#include "PCH.h"
#include "Bitmap.h"
#include "Logger.h"
#include "Timer.h"
#include "Math/Half.h"

#include "../External/tinyexr/tinyexr.h"

namespace rt {

using namespace math;

bool Bitmap::LoadEXR(FILE* file, const char* path)
{
    RT_UNUSED(file);

    // 1. Read EXR version.
    EXRVersion exrVersion;

    int ret = ParseEXRVersionFromFile(&exrVersion, path);
    if (ret != 0)
    {
        RT_LOG_ERROR("Invalid EXR file: %s", path);
        return false;
    }

    if (exrVersion.multipart)
    {
        RT_LOG_ERROR("Multipart EXR are not supported: %s", path);
        return false;
    }

    // 2. Read EXR header
    EXRHeader exrHeader;
    InitEXRHeader(&exrHeader);

    const char* err = NULL;
    ret = ParseEXRHeaderFromFile(&exrHeader, &exrVersion, path, &err);
    if (ret != 0)
    {
        RT_LOG_ERROR("Parse EXR error: %s", err);
        FreeEXRErrorMessage(err);
        return ret;
    }

    EXRImage exrImage;
    InitEXRImage(&exrImage);

    ret = LoadEXRImageFromFile(&exrImage, &exrHeader, path, &err);
    if (ret != 0)
    {
        RT_LOG_ERROR("Load EXR error: %s", err);
        FreeEXRErrorMessage(err);
        return ret;
    }

    if (!exrImage.images)
    {
        RT_LOG_ERROR("Tiled EXR are not supported: %s", path);
        FreeEXRImage(&exrImage);
        return false;
    }

    if (exrHeader.num_channels == 3)
    {
        const bool sameFormat = exrHeader.pixel_types[0] == exrHeader.pixel_types[1] && exrHeader.pixel_types[0] == exrHeader.pixel_types[2];
        if (!sameFormat)
        {
            RT_LOG_ERROR("Unsupported EXR format. All channels must be of the same type");
            goto exrImageError;
        }

        const size_t numPixels = static_cast<size_t>(exrImage.width) * static_cast<size_t>(exrImage.height);

        if (exrHeader.pixel_types[0] == TINYEXR_PIXELTYPE_FLOAT)
        {
            if (!Init(exrImage.width, exrImage.height, Format::R32G32B32_Float, nullptr, true))
            {
                goto exrImageError;
            }

            Float* typedData = reinterpret_cast<Float*>(mData);
            for (size_t i = 0; i < numPixels; ++i)
            {
                typedData[3 * i    ] = reinterpret_cast<const Float*>(exrImage.images[2])[i];
                typedData[3 * i + 1] = reinterpret_cast<const Float*>(exrImage.images[1])[i];
                typedData[3 * i + 2] = reinterpret_cast<const Float*>(exrImage.images[0])[i];
            }
        }
        else if (exrHeader.pixel_types[0] == TINYEXR_PIXELTYPE_HALF)
        {
            if (!Init(exrImage.width, exrImage.height, Format::R16G16B16_Half, nullptr, true))
            {
                goto exrImageError;
            }

            Uint16* typedData = reinterpret_cast<Uint16*>(mData);
            for (size_t i = 0; i < numPixels; ++i)
            {
                typedData[3 * i    ] = reinterpret_cast<const Uint16*>(exrImage.images[2])[i];
                typedData[3 * i + 1] = reinterpret_cast<const Uint16*>(exrImage.images[1])[i];
                typedData[3 * i + 2] = reinterpret_cast<const Uint16*>(exrImage.images[0])[i];
            }
        }
        else
        {
            RT_LOG_ERROR("Unsupported EXR format: %i", exrHeader.pixel_types[0]);
            goto exrImageError;
        }
    }
    else
    {
        RT_LOG_ERROR("Unsupported EXR format.", path);
        goto exrImageError;
    }

    // 4. Free image data
    FreeEXRImage(&exrImage);
    return true;

exrImageError:
    FreeEXRImage(&exrImage);
    return false;
}

bool Bitmap::SaveEXR(const char* path, const Float exposure) const
{
    if (mFormat != Format::R32G32B32_Float)
    {
        RT_LOG_ERROR("Bitmap::SaveEXR: Unsupported format");
        return false;
    }

    // TODO support more types

    const Float3* data = reinterpret_cast<const Float3*>(mData);

    EXRHeader header;
    InitEXRHeader(&header);

    EXRImage image;
    InitEXRImage(&image);

    image.num_channels = 3;

    std::vector<float> images[3];
    images[0].resize(mWidth * mHeight);
    images[1].resize(mWidth * mHeight);
    images[2].resize(mWidth * mHeight);

    // Split RGBRGBRGB... into R, G and B layer
    const Uint32 numPixels = GetWidth() * GetHeight();
    for (Uint32 i = 0; i < numPixels; i++)
    {
        images[0][i] = exposure * data[i].x;
        images[1][i] = exposure * data[i].y;
        images[2][i] = exposure * data[i].z;
    }

    float* image_ptr[3];
    image_ptr[0] = images[2].data(); // B
    image_ptr[1] = images[1].data(); // G
    image_ptr[2] = images[0].data(); // R

    image.images = (unsigned char**)image_ptr;
    image.width = mWidth;
    image.height = mHeight;

    header.compression_type = TINYEXR_COMPRESSIONTYPE_PIZ;
    header.num_channels = 3;
    header.channels = (EXRChannelInfo*)malloc(sizeof(EXRChannelInfo) * header.num_channels);

    // Must be (A)BGR order, since most of EXR viewers expect this channel order.
    {
        strcpy(header.channels[0].name, "B");
        strcpy(header.channels[1].name, "G");
        strcpy(header.channels[2].name, "R");
    }

    header.pixel_types = (int*)malloc(sizeof(int) * header.num_channels);
    header.requested_pixel_types = (int*)malloc(sizeof(int) * header.num_channels);
    for (int i = 0; i < header.num_channels; i++)
    {
        header.pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT; // pixel type of input image
        header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT; // pixel type of output image to be stored in .EXR
    }

    const char* err = nullptr;
    int ret = SaveEXRImageToFile(&image, &header, path, &err);
    if (ret != TINYEXR_SUCCESS)
    {
        RT_LOG_ERROR("Failed to save EXR file '%s': %s", path, err);
        FreeEXRErrorMessage(err);

        free(header.channels);
        free(header.pixel_types);
        free(header.requested_pixel_types);

        return ret;
    }

    RT_LOG_INFO("Image file '%s' written successfully", path);

    free(header.channels);
    free(header.pixel_types);
    free(header.requested_pixel_types);

    return true;
}

} // namespace rt
