#include "point_measurement/tiff_roi_reader.hpp"

#include <tiffio.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace rpc_localization {
namespace {

TIFF* open_tiff(const std::filesystem::path& path) {
    // Windows 下使用宽字符接口，保证中文路径可以正常打开。
#ifdef _WIN32
    return TIFFOpenW(path.wstring().c_str(), "r");
#else
    return TIFFOpen(path.string().c_str(), "r");
#endif
}

void validate_single_float_image(TIFF* tif) {
    // 当前项目只按单波段 float32 设计。遇到其他位深或波段数直接报错，
    // 避免在不知情的情况下把数据隐式转成 8 位或 16 位。
    std::uint16_t samples_per_pixel = 0;
    std::uint16_t bits_per_sample = 0;
    std::uint16_t sample_format = SAMPLEFORMAT_UINT;

    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
    TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &sample_format);

    if (samples_per_pixel != 1 || bits_per_sample != 32 ||
        sample_format != SAMPLEFORMAT_IEEEFP) {
        throw std::runtime_error("当前只支持单波段 float32 TIFF");
    }
    if (TIFFIsTiled(tif) != 0) {
        throw std::runtime_error("当前读取器只支持条带 TIFF");
    }
}

}  // namespace

TiffInfo read_tiff_info(const std::filesystem::path& path) {
    // 只读取尺寸标签，不加载整幅影像。
    TIFF* tif = open_tiff(path);
    if (tif == nullptr) {
        throw std::runtime_error("无法打开 TIFF: " + path.u8string());
    }

    std::uint32_t width = 0;
    std::uint32_t height = 0;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFClose(tif);

    return TiffInfo{static_cast<int>(width), static_cast<int>(height)};
}

cv::Mat_<float> read_tiff_roi(
    const std::filesystem::path& path,
    int x0,
    int y0,
    int width,
    int height) {
    // x0、y0 是原影像左上角 0 基坐标，width、height 是像素宽高。
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("ROI 尺寸必须为正数");
    }

    TIFF* tif = open_tiff(path);
    if (tif == nullptr) {
        throw std::runtime_error("无法打开 TIFF: " + path.u8string());
    }

    validate_single_float_image(tif);

    std::uint32_t image_width = 0;
    std::uint32_t image_height = 0;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &image_width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &image_height);

    if (x0 < 0 || y0 < 0 || x0 + width > static_cast<int>(image_width) ||
        y0 + height > static_cast<int>(image_height)) {
        TIFFClose(tif);
        throw std::runtime_error("ROI 超出影像范围");
    }

    const tmsize_t scanline_size = TIFFScanlineSize(tif);
    if (scanline_size < static_cast<tmsize_t>(image_width) * 4) {
        TIFFClose(tif);
        throw std::runtime_error("TIFF 扫描行大小不符合 float32 单波段预期");
    }

    cv::Mat_<float> result(height, width);
    std::vector<std::uint8_t> row(static_cast<std::size_t>(scanline_size));

    for (int row_index = 0; row_index < height; ++row_index) {
        // libtiff 按行读取，因此这里只缓存一行，不占整幅影像内存。
        const int source_line = y0 + row_index;
        if (TIFFReadScanline(tif, row.data(), static_cast<std::uint32_t>(source_line)) < 0) {
            TIFFClose(tif);
            throw std::runtime_error("读取 TIFF 扫描行失败");
        }

        // 从当前扫描行中截取需要的横向范围，再复制到 OpenCV 矩阵。
        const float* source = reinterpret_cast<const float*>(
            row.data() + static_cast<std::size_t>(x0) * sizeof(float));
        std::copy(source, source + width, result.ptr<float>(row_index));
    }

    TIFFClose(tif);
    return result;
}

}  // namespace rpc_localization
