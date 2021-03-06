#include "PCH.h"
#include "Bitmap.h"
#include "Logger.h"
#include "BlockCompression.h"
#include "Timer.h"
#include "Math/Half.h"

#include <string.h>

namespace rt {

using namespace math;

Uint32 Bitmap::BitsPerPixel(Format format)
{
    switch (format)
    {
    case Format::Unknown:               return 0;
    case Format::R8_Uint:               return 8;
    case Format::B8G8R8_Uint:           return 8 * 3;
    case Format::B8G8R8A8_Uint:         return 8 * 4;
    case Format::R32G32B32_Float:       return 8 * sizeof(Float) * 3;
    case Format::R32G32B32A32_Float:    return 8 * sizeof(Float) * 4;
    case Format::R16G16B16_Half:        return 8 * sizeof(Uint16) * 3;
    case Format::BC1:                   return 4;
    case Format::BC4:                   return 4;
    case Format::BC5:                   return 8;
    }

    return 0;
}

const char* Bitmap::FormatToString(Format format)
{
    switch (format)
    {
    case Format::R8_Uint:               return "R8_Uint";
    case Format::B8G8R8_Uint:           return "B8G8R8_Uint";
    case Format::B8G8R8A8_Uint:         return "B8G8R8A8_Uint";
    case Format::R32G32B32_Float:       return "R32G32B32_Float";
    case Format::R32G32B32A32_Float:    return "R32G32B32A32_Float";
    case Format::R16G16B16_Half:        return "R16G16B16_Half";
    case Format::BC1:                   return "BC1";
    case Format::BC4:                   return "BC4";
    case Format::BC5:                   return "BC5";
    }

    return "<unknown>";
}

size_t Bitmap::GetDataSize(Uint32 width, Uint32 height, Format format)
{
    const Uint64 dataSize = (Uint64)width * (Uint64)height * (Uint64)BitsPerPixel(format) / 8;

    if (dataSize >= (Uint64)std::numeric_limits<size_t>::max())
    {
        return std::numeric_limits<size_t>::max();
    }

    return (size_t)dataSize;
}

Bitmap::Bitmap(const char* debugName)
    : mData(nullptr)
    , mWidth(0)
    , mHeight(0)
    , mFormat(Format::Unknown)
    , mLinearSpace(false)
    , mDebugName(debugName)
{ }

Bitmap::~Bitmap()
{
    Release();

    RT_LOG_INFO("Releasing bitmap '%s'", mDebugName.c_str());
}

void Bitmap::Clear()
{
    if (mData)
    {
        memset(mData, 0, GetDataSize(mWidth, mHeight, mFormat));
    }
}

void Bitmap::Release()
{
    if (mData)
    {
        AlignedFree(mData);
        mData = nullptr;
    }

    mWidth = 0;
    mHeight = 0;
    mFormat = Format::Unknown;
}

bool Bitmap::Init(Uint32 width, Uint32 height, Format format, const void* data, bool linearSpace)
{
    const size_t dataSize = GetDataSize(width, height, format);
    if (dataSize == 0)
    {
        RT_LOG_ERROR("Invalid bitmap format");
        return false;
    }
    if (dataSize == std::numeric_limits<size_t>::max())
    {
        RT_LOG_ERROR("Texture is too big");
        return false;
    }

    Release();

    // align to cache line
    const Uint32 marigin = RT_CACHE_LINE_SIZE;
    mData = (Uint8*)AlignedMalloc(dataSize + marigin, RT_CACHE_LINE_SIZE);
    if (!mData)
    {
        RT_LOG_ERROR("Memory allocation failed");
        return false;
    }

    if (data)
    {
        memcpy(mData, data, dataSize);
    }

    mWidth = (Uint16)width;
    mHeight = (Uint16)height;
    mFloatSize = Vector4((Float)width, (Float)height, (Float)width, (Float)height);
    mSize = VectorInt4(width, height, width, height);
    mFormat = format;
    mLinearSpace = linearSpace;

    return true;
}

bool Bitmap::Copy(Bitmap& target, const Bitmap& source)
{
    if (target.mWidth != source.mWidth || target.mHeight != source.mHeight)
    {
        RT_LOG_ERROR("Bitmap copy failed: bitmaps have different dimensions");
        return false;
    }

    if (target.mFormat != source.mFormat)
    {
        RT_LOG_ERROR("Bitmap copy failed: bitmaps have different formats");
        return false;
    }

    memcpy(target.GetData(), source.GetData(), GetDataSize(target.mWidth, target.mHeight, target.mFormat));
    return true;
}

bool Bitmap::Load(const char* path)
{
    Timer timer;

    FILE* file = fopen(path, "rb");
    if (!file)
    {
        RT_LOG_ERROR("Failed to load source image from file '%hs'", path);
        return false;
    }

    if (!LoadBMP(file, path))
    {
        fseek(file, SEEK_SET, 0);

        if (!LoadDDS(file, path))
        {
            fseek(file, SEEK_SET, 0);

            if (!LoadEXR(file, path))
            {
                RT_LOG_ERROR("Failed to load '%hs' - unknown format", path);
                fclose(file);
                return false;
            }
        }
    }

    fclose(file);

    const float elapsedTime = static_cast<float>(1000.0 * timer.Stop());
    RT_LOG_INFO("Bitmap '%hs' loaded in %.3fms: format=%s, width=%u, height=%u", path, elapsedTime, FormatToString(mFormat), mWidth, mHeight);
    return true;
}

Vector4 Bitmap::GetPixel(Uint32 x, Uint32 y, const bool forceLinearSpace) const
{
    RT_ASSERT(x < mWidth);
    RT_ASSERT(y < mHeight);

    const Uint32 offset = mWidth * y + x;

    Vector4 color;
    switch (mFormat)
    {
        case Format::R8_Uint:
        {
            const Uint32 value = mData[offset];
            color = Vector4::FromInteger(value) * (1.0f / 255.0f);
            break;
        }

        case Format::B8G8R8_Uint:
        {
            const Uint8* source = mData + (3 * offset);
            color = Vector4::LoadBGR_UNorm(source);
            break;
        }

        case Format::B8G8R8A8_Uint:
        {
            const Uint8* source = mData + (4 * offset);
            color = Vector4::Load4(source).Swizzle<2, 1, 0, 3>() * (1.0f / 255.0f);
            break;
        }

        case Format::R32G32B32_Float:
        {
            const float* source = reinterpret_cast<const float*>(mData) + 3 * offset;
            color = Vector4(source) & Vector4::MakeMask<1,1,1,0>();
            break;
        }

        case Format::R32G32B32A32_Float:
        {
            const Vector4* source = reinterpret_cast<const Vector4*>(mData) + offset;
            RT_PREFETCH_L2(source - mWidth);
            RT_PREFETCH_L2(source + mWidth);
            color = *source;
            break;
        }

        case Format::R16G16B16_Half:
        {
            const Half* source = reinterpret_cast<const Half*>(mData) + 3 * offset;
            color = Vector4::FromHalves(source) & Vector4::MakeMask<1,1,1,0>();
            break;
        }

        case Format::BC1:
        {
            const Uint32 flippedY = mHeight - 1 - y;
            color = DecodeBC1(reinterpret_cast<const Uint8*>(mData), x, flippedY, mWidth);
            break;
        }

        case Format::BC4:
        {
            const Uint32 flippedY = mHeight - 1 - y;
            color = DecodeBC4(reinterpret_cast<const Uint8*>(mData), x, flippedY, mWidth);
            break;
        }

        case Format::BC5:
        {
            const Uint32 flippedY = mHeight - 1 - y;
            color = DecodeBC5(reinterpret_cast<const Uint8*>(mData), x, flippedY, mWidth);
            break;
        }

        default:
        {
            RT_FATAL("Unsupported bitmap format");
            color = Vector4::Zero();
        }
    }

    if (!mLinearSpace && !forceLinearSpace)
    {
        color *= color;
    }

    return color;
}

void Bitmap::GetPixelBlock(const math::VectorInt4 coords, const bool forceLinearSpace,
    math::Vector4& outColor0, math::Vector4& outColor1, math::Vector4& outColor2, math::Vector4& outColor3) const
{
    RT_ASSERT(coords.x < mWidth);
    RT_ASSERT(coords.y < mHeight);
    RT_ASSERT(coords.z < mWidth);
    RT_ASSERT(coords.w < mHeight);

    // calculate offsets in pixels array for each corner
    const VectorInt4 offsets = coords.Swizzle<1,1,3,3>() * (Int32)mWidth + coords.Swizzle<0,2,0,2>();

    constexpr float byteScale = 1.0f / 255.0f;

    switch (mFormat)
    {
        case Format::R8_Uint:
        {
            const Uint32 value0 = mData[offsets.x];
            const Uint32 value1 = mData[offsets.y];
            const Uint32 value2 = mData[offsets.z];
            const Uint32 value3 = mData[offsets.w];
            outColor0 = Vector4::FromInteger(value0) * byteScale;
            outColor1 = Vector4::FromInteger(value1) * byteScale;
            outColor2 = Vector4::FromInteger(value2) * byteScale;
            outColor3 = Vector4::FromInteger(value3) * byteScale;
            break;
        }

        case Format::B8G8R8_Uint:
        {
            outColor0 = Vector4::LoadBGR_UNorm(mData + 3u * (Uint32)offsets.x);
            outColor1 = Vector4::LoadBGR_UNorm(mData + 3u * (Uint32)offsets.y);
            outColor2 = Vector4::LoadBGR_UNorm(mData + 3u * (Uint32)offsets.z);
            outColor3 = Vector4::LoadBGR_UNorm(mData + 3u * (Uint32)offsets.w);
            break;
        }

        case Format::B8G8R8A8_Uint:
        {
            outColor0 = Vector4::Load4(mData + 4 * (Uint32)offsets.x).Swizzle<2, 1, 0, 3>() * byteScale;
            outColor1 = Vector4::Load4(mData + 4 * (Uint32)offsets.y).Swizzle<2, 1, 0, 3>() * byteScale;
            outColor2 = Vector4::Load4(mData + 4 * (Uint32)offsets.z).Swizzle<2, 1, 0, 3>() * byteScale;
            outColor3 = Vector4::Load4(mData + 4 * (Uint32)offsets.w).Swizzle<2, 1, 0, 3>() * byteScale;
            break;
        }

        case Format::R32G32B32_Float:
        {
            const float* source0 = reinterpret_cast<const float*>(mData) + 3u * (Uint32)offsets.x;
            const float* source1 = reinterpret_cast<const float*>(mData) + 3u * (Uint32)offsets.y;
            const float* source2 = reinterpret_cast<const float*>(mData) + 3u * (Uint32)offsets.z;
            const float* source3 = reinterpret_cast<const float*>(mData) + 3u * (Uint32)offsets.w;
            outColor0 = Vector4(source0) & Vector4::MakeMask<1,1,1,0>();
            outColor1 = Vector4(source1) & Vector4::MakeMask<1,1,1,0>();
            outColor2 = Vector4(source2) & Vector4::MakeMask<1,1,1,0>();
            outColor3 = Vector4(source3) & Vector4::MakeMask<1,1,1,0>();
            break;
        }

        case Format::R32G32B32A32_Float:
        {
            outColor0 = reinterpret_cast<const Vector4*>(mData)[offsets.x];
            outColor1 = reinterpret_cast<const Vector4*>(mData)[offsets.y];
            outColor2 = reinterpret_cast<const Vector4*>(mData)[offsets.z];
            outColor3 = reinterpret_cast<const Vector4*>(mData)[offsets.w];
            break;
        }

        case Format::R16G16B16_Half:
        {
            const Half* source0 = reinterpret_cast<const Half*>(mData) + 3u * (Uint32)offsets.x;
            const Half* source1 = reinterpret_cast<const Half*>(mData) + 3u * (Uint32)offsets.y;
            const Half* source2 = reinterpret_cast<const Half*>(mData) + 3u * (Uint32)offsets.z;
            const Half* source3 = reinterpret_cast<const Half*>(mData) + 3u * (Uint32)offsets.w;
            outColor0 = Vector4::FromHalves(source0) & Vector4::MakeMask<1,1,1,0>();
            outColor1 = Vector4::FromHalves(source1) & Vector4::MakeMask<1,1,1,0>();
            outColor2 = Vector4::FromHalves(source2) & Vector4::MakeMask<1,1,1,0>();
            outColor3 = Vector4::FromHalves(source3) & Vector4::MakeMask<1,1,1,0>();
            break;
        }

        case Format::BC1:
        {
            const Int32 heightMinusOne = (Int32)mHeight - 1;
            const VectorInt4 flippedCoords = VectorInt4(heightMinusOne) - coords;
            outColor0 = DecodeBC1(mData, coords.x, flippedCoords.y, mWidth);
            outColor1 = DecodeBC1(mData, coords.z, flippedCoords.y, mWidth);
            outColor2 = DecodeBC1(mData, coords.x, flippedCoords.w, mWidth);
            outColor3 = DecodeBC1(mData, coords.z, flippedCoords.w, mWidth);
            break;
        }

        /*
        case Format::BC4:
        {
            const Uint32 flippedY = mHeight - 1 - y;
            color = DecodeBC4(reinterpret_cast<const Uint8*>(mData), x, flippedY, mWidth);
            break;
        }

        case Format::BC5:
        {
            const Uint32 flippedY = mHeight - 1 - y;
            color = DecodeBC5(reinterpret_cast<const Uint8*>(mData), x, flippedY, mWidth);
            break;
        }
        */

        default:
        {
            RT_FATAL("Unsupported bitmap format");
            outColor0 = Vector4::Zero();
            outColor1 = Vector4::Zero();
            outColor2 = Vector4::Zero();
            outColor3 = Vector4::Zero();
        }
    }

    if (!mLinearSpace && !forceLinearSpace)
    {
        outColor0 *= outColor0;
        outColor1 *= outColor1;
        outColor2 *= outColor2;
        outColor3 *= outColor3;
    }
}

Vector4 Bitmap::Sample(Vector4 coords, const SamplerDesc& sampler) const
{
    VectorInt4 intCoords = VectorInt4::Convert(Vector4::Floor(coords));

    // perform wrapping
    coords -= intCoords.ConvertToFloat();
    coords *= mFloatSize;
    intCoords = VectorInt4::Convert(Vector4::Floor(coords));

    Vector4 result;

    if (sampler.filter == TextureFilterMode::NearestNeighbor)
    {
        result = GetPixel(intCoords.x, intCoords.y, sampler.forceLinearSpace);
    }
    else if (sampler.filter == TextureFilterMode::Bilinear)
    {
        intCoords = intCoords.Swizzle<0, 1, 0, 1>();
        intCoords += VectorInt4(0, 0, 1, 1);

        // wrap secondary coordinates
        intCoords = intCoords.SetIfGreaterOrEqual(mSize, intCoords - mSize);

        Vector4 value00, value01, value10, value11;
        GetPixelBlock(intCoords, sampler.forceLinearSpace, value00, value10, value01, value11);

        // bilinear interpolation
        const Vector4 weights = coords - intCoords.ConvertToFloat();
        const Vector4 value0 = Vector4::Lerp(value00, value01, weights.SplatY());
        const Vector4 value1 = Vector4::Lerp(value10, value11, weights.SplatY());
        result = Vector4::Lerp(value0, value1, weights.SplatX());
    }
    else
    {
        RT_FATAL("Invalid filter mode");
        result = Vector4::Zero();
    }

    RT_ASSERT(result.IsValid());

    return result;
}

} // namespace rt
