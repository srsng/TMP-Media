#include "pch.h"
#include "MediaCoverImage.h"

#include <algorithm>
#include <cstdint>
#include <limits>

#include <shcore.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <winrt/Windows.Foundation.h>

namespace
{
    using Microsoft::WRL::ComPtr;

    constexpr UINT kMaximumCoverEdge = 256;

    ComPtr<IWICImagingFactory> CreateImagingFactory()
    {
        ComPtr<IWICImagingFactory> factory;
        HRESULT result = CoCreateInstance(
            CLSID_WICImagingFactory2,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory));
        if (FAILED(result))
        {
            winrt::check_hresult(CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&factory)));
        }
        return factory;
    }

    std::pair<UINT, UINT> CalculateDecodedSize(UINT width, UINT height) noexcept
    {
        const UINT longest_edge = (std::max)(width, height);
        if (longest_edge <= kMaximumCoverEdge)
        {
            return { width, height };
        }

        const UINT scaled_width = (std::max)(
            1U,
            static_cast<UINT>(static_cast<std::uint64_t>(width) * kMaximumCoverEdge / longest_edge));
        const UINT scaled_height = (std::max)(
            1U,
            static_cast<UINT>(static_cast<std::uint64_t>(height) * kMaximumCoverEdge / longest_edge));
        return { scaled_width, scaled_height };
    }
}

media::MediaCoverImage::MediaCoverImage(HBITMAP bitmap, int width, int height) noexcept
    : m_bitmap(bitmap)
    , m_width(width)
    , m_height(height)
{
}

media::MediaCoverImage::~MediaCoverImage()
{
    if (m_bitmap != nullptr)
    {
        DeleteObject(m_bitmap);
    }
}

std::shared_ptr<const media::MediaCoverImage> media::DecodeMediaCover(
    const winrt::Windows::Storage::Streams::IRandomAccessStreamReference& reference) noexcept
{
    try
    {
        if (!reference)
        {
            return {};
        }

        const auto random_access_stream = reference.OpenReadAsync().get();
        if (!random_access_stream)
        {
            return {};
        }

        ComPtr<IStream> stream;
        winrt::check_hresult(CreateStreamOverRandomAccessStream(
            winrt::get_unknown(random_access_stream),
            IID_PPV_ARGS(&stream)));

        const ComPtr<IWICImagingFactory> factory = CreateImagingFactory();
        ComPtr<IWICBitmapDecoder> decoder;
        winrt::check_hresult(factory->CreateDecoderFromStream(
            stream.Get(),
            nullptr,
            WICDecodeMetadataCacheOnLoad,
            &decoder));

        ComPtr<IWICBitmapFrameDecode> frame;
        winrt::check_hresult(decoder->GetFrame(0, &frame));

        UINT source_width{};
        UINT source_height{};
        winrt::check_hresult(frame->GetSize(&source_width, &source_height));
        if (source_width == 0 || source_height == 0)
        {
            return {};
        }

        const auto [decoded_width, decoded_height] = CalculateDecodedSize(source_width, source_height);
        ComPtr<IWICBitmapSource> bitmap_source;
        if (decoded_width != source_width || decoded_height != source_height)
        {
            ComPtr<IWICBitmapScaler> scaler;
            winrt::check_hresult(factory->CreateBitmapScaler(&scaler));
            winrt::check_hresult(scaler->Initialize(
                frame.Get(),
                decoded_width,
                decoded_height,
                WICBitmapInterpolationModeFant));
            winrt::check_hresult(scaler.As(&bitmap_source));
        }
        else
        {
            winrt::check_hresult(frame.As(&bitmap_source));
        }

        ComPtr<IWICFormatConverter> converter;
        winrt::check_hresult(factory->CreateFormatConverter(&converter));
        winrt::check_hresult(converter->Initialize(
            bitmap_source.Get(),
            GUID_WICPixelFormat32bppBGR,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom));

        if (decoded_width > static_cast<UINT>((std::numeric_limits<int>::max)())
            || decoded_height > static_cast<UINT>((std::numeric_limits<int>::max)()))
        {
            return {};
        }

        BITMAPINFO bitmap_info{};
        bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmap_info.bmiHeader.biWidth = static_cast<LONG>(decoded_width);
        bitmap_info.bmiHeader.biHeight = -static_cast<LONG>(decoded_height);
        bitmap_info.bmiHeader.biPlanes = 1;
        bitmap_info.bmiHeader.biBitCount = 32;
        bitmap_info.bmiHeader.biCompression = BI_RGB;

        void* pixels{};
        HBITMAP bitmap = CreateDIBSection(
            nullptr,
            &bitmap_info,
            DIB_RGB_COLORS,
            &pixels,
            nullptr,
            0);
        if (bitmap == nullptr || pixels == nullptr)
        {
            if (bitmap != nullptr)
            {
                DeleteObject(bitmap);
            }
            return {};
        }

        const UINT stride = decoded_width * 4;
        const UINT buffer_size = stride * decoded_height;
        const HRESULT copy_result = converter->CopyPixels(nullptr, stride, buffer_size, static_cast<BYTE*>(pixels));
        if (FAILED(copy_result))
        {
            DeleteObject(bitmap);
            winrt::check_hresult(copy_result);
        }

        return std::make_shared<MediaCoverImage>(
            bitmap,
            static_cast<int>(decoded_width),
            static_cast<int>(decoded_height));
    }
    catch (...)
    {
        return {};
    }
}
