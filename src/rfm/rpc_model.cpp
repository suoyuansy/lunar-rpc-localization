#include "rfm/rpc_model.hpp"
#include "common/file_io.hpp"

#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace rpc_localization {
namespace {

std::string trim(const std::string& value) {
    // 去掉 RPC 文本首尾空白，便于按 key:value 解析。
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

double parse_value(const std::string& text) {
    // RPC 行末可能带单位，例如 "pixels" 或 "meters"，这里只取第一个数值。
    std::istringstream stream(text);
    double value = 0.0;
    if (!(stream >> value)) {
        throw std::runtime_error("无法解析 RPC 数值: " + text);
    }
    return value;
}

void assign_coefficient(
    std::unordered_map<std::string, double>& values,
    const std::array<double, 20>& coefficients,
    const std::string& prefix) {
    // 将系数数组转成带编号的键，方便统一查找。
    for (int i = 0; i < 20; ++i) {
        values[prefix + "_COEFF_" + std::to_string(i + 1)] = coefficients[i];
    }
}

double basis_dot(
    const std::array<double, 20>& coefficients,
    const std::array<double, 20>& basis) {
    // 计算 20 项有理函数系数的线性组合。
    double result = 0.0;
    for (std::size_t i = 0; i < coefficients.size(); ++i) {
        result += coefficients[i] * basis[i];
    }
    return result;
}

}  // namespace

RpcModel RpcModel::from_file(const std::filesystem::path& path) {
    // RPC 文件采用 "KEY: VALUE" 格式；这里先全部读入，再按字段检查。
    std::ifstream input = open_input_file(path);
    if (!input) {
        throw std::runtime_error("无法打开 RPC 文件: " + path.u8string());
    }

    std::unordered_map<std::string, double> values;
    std::string line;
    while (std::getline(input, line)) {
        const auto separator = line.find(':');
        if (separator == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, separator));
        const std::string value_text = trim(line.substr(separator + 1));
        values[key] = parse_value(value_text);
    }

    RpcModel model;
    auto require = [&](const std::string& key) {
        const auto it = values.find(key);
        if (it == values.end()) {
            throw std::runtime_error("RPC 缺少字段: " + key);
        }
        return it->second;
    };

    model.line_off_ = require("LINE_OFF");
    model.line_scale_ = require("LINE_SCALE");
    model.samp_off_ = require("SAMP_OFF");
    model.samp_scale_ = require("SAMP_SCALE");
    model.lat_off_ = require("LAT_OFF");
    model.lat_scale_ = require("LAT_SCALE");
    model.long_off_ = require("LONG_OFF");
    model.long_scale_ = require("LONG_SCALE");
    model.height_off_ = require("HEIGHT_OFF");
    model.height_scale_ = require("HEIGHT_SCALE");

    for (int i = 0; i < 20; ++i) {
        // 分子和分母各 20 个系数，编号从 1 开始。
        const auto index = std::to_string(i + 1);
        model.line_num_[i] = require("LINE_NUM_COEFF_" + index);
        model.line_den_[i] = require("LINE_DEN_COEFF_" + index);
        model.samp_num_[i] = require("SAMP_NUM_COEFF_" + index);
        model.samp_den_[i] = require("SAMP_DEN_COEFF_" + index);
    }

    if (model.line_scale_ == 0.0 || model.samp_scale_ == 0.0 ||
        model.lat_scale_ == 0.0 || model.long_scale_ == 0.0 ||
        model.height_scale_ == 0.0) {
        throw std::runtime_error("RPC 归一化尺度不能为 0");
    }

    return model;
}

PixelPoint RpcModel::forward(const GeoPoint& point) const {
    // 先把经纬度高程归一化，再构造 20 项基函数。
    const double longitude = (point.longitude_deg - long_off_) / long_scale_;
    const double latitude = (point.latitude_deg - lat_off_) / lat_scale_;
    const double height = (point.height_m - height_off_) / height_scale_;

    // 基函数顺序必须与 RPC 系数编号和标准公式严格一致，
    // 顺序错误会直接导致正算像点错误。
    const std::array<double, 20> basis = {
        1.0,
        longitude,
        latitude,
        height,
        longitude * latitude,
        longitude * longitude,
        latitude * latitude,
        height * height,
        latitude * height,
        longitude * height,
        longitude * longitude * longitude,
        longitude * latitude * latitude,
        longitude * height * height,
        longitude * longitude * latitude,
        latitude * latitude * latitude,
        latitude * height * height,
        longitude * longitude * height,
        latitude * latitude * height,
        height * height * height,
        longitude * latitude * height,
    };

    const double line_num = basis_dot(line_num_, basis);
    const double line_den = basis_dot(line_den_, basis);
    const double samp_num = basis_dot(samp_num_, basis);
    const double samp_den = basis_dot(samp_den_, basis);

    if (std::abs(line_den) < 1e-15 || std::abs(samp_den) < 1e-15) {
        throw std::runtime_error("RPC 分母接近 0");
    }

    PixelPoint result;
    result.line = line_off_ + line_scale_ * line_num / line_den;
    result.sample = samp_off_ + samp_scale_ * samp_num / samp_den;
    return result;
}

}  // namespace rpc_localization
