#include "rfm/rfm_solver.hpp"

#include <opencv2/core.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace rpc_localization {
namespace {

using ResidualFunction =
    std::function<std::vector<double>(const std::vector<double>&)>;
using JacobianFunction =
    std::function<std::vector<double>(const std::vector<double>&)>;

// LM 内部结果，最后会转换成对外的 RfmSolution。
struct LmResult {
    std::vector<double> x;
    bool converged = false;
    int iterations = 0;
    double residual_rms = 0.0;
    double condition_number = 0.0;
};

double squared_norm(const std::vector<double>& values) {
    // 残差平方和，是 LM 的目标函数。
    double result = 0.0;
    for (double value : values) {
        result += value * value;
    }
    return result;
}

double vector_norm(const std::vector<double>& values) {
    return std::sqrt(squared_norm(values));
}

bool solve_linear_system(
    std::vector<double> matrix,
    std::vector<double> right_hand_side,
    std::vector<double>& solution) {
    // 使用带主元选择的高斯消元求解 2×2 或 3×3 小矩阵。
    // 主元过小说明方程病态或接近奇异，返回 false。
    const int size = static_cast<int>(right_hand_side.size());
    for (int column = 0; column < size; ++column) {
        int pivot = column;
        double pivot_value = std::abs(matrix[column * size + column]);
        for (int row = column + 1; row < size; ++row) {
            const double candidate =
                std::abs(matrix[row * size + column]);
            if (candidate > pivot_value) {
                pivot = row;
                pivot_value = candidate;
            }
        }

        if (!std::isfinite(pivot_value) || pivot_value < 1e-14) {
            return false;
        }

        if (pivot != column) {
            for (int k = 0; k < size; ++k) {
                std::swap(
                    matrix[column * size + k],
                    matrix[pivot * size + k]);
            }
            std::swap(right_hand_side[column], right_hand_side[pivot]);
        }

        for (int row = column + 1; row < size; ++row) {
            const double factor =
                matrix[row * size + column] /
                matrix[column * size + column];
            for (int k = column; k < size; ++k) {
                matrix[row * size + k] -=
                    factor * matrix[column * size + k];
            }
            right_hand_side[row] -= factor * right_hand_side[column];
        }
    }

    solution.assign(size, 0.0);
    for (int row = size - 1; row >= 0; --row) {
        double value = right_hand_side[row];
        for (int column = row + 1; column < size; ++column) {
            value -= matrix[row * size + column] * solution[column];
        }
        const double diagonal = matrix[row * size + row];
        if (!std::isfinite(diagonal) || std::abs(diagonal) < 1e-14) {
            return false;
        }
        solution[row] = value / diagonal;
    }
    return true;
}

double condition_number(
    const std::vector<double>& jacobian,
    int rows,
    int columns) {
    // 对雅可比矩阵做 SVD，用最大奇异值除以最小奇异值得到条件数。
    if (jacobian.empty() || rows <= 0 || columns <= 0) {
        return std::numeric_limits<double>::infinity();
    }

    cv::Mat matrix(rows, columns, CV_64F);
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            matrix.at<double>(row, column) =
                jacobian[row * columns + column];
        }
    }

    cv::Mat singular_values;
    cv::SVD::compute(matrix, singular_values);

    double maximum = 0.0;
    double minimum = std::numeric_limits<double>::infinity();
    for (int i = 0; i < singular_values.rows; ++i) {
        const double value = singular_values.at<double>(i, 0);
        maximum = std::max(maximum, value);
        minimum = std::min(minimum, value);
    }

    if (!std::isfinite(minimum) || minimum < 1e-14) {
        return std::numeric_limits<double>::infinity();
    }
    return maximum / minimum;
}

LmResult solve_lm(
    std::vector<double> initial,
    int residual_rows,
    int parameter_columns,
    const ResidualFunction& residual_function,
    const JacobianFunction& jacobian_function) {
    // LM 主迭代：
    // 1. 计算残差和雅可比；
    // 2. 构造 J^T J 和梯度 J^T r；
    // 3. 加阻尼后求解增量；
    // 4. 只有残差下降才接受该步，否则增大阻尼重试。
    constexpr int max_iterations = 100;
    constexpr double residual_tolerance = 1e-6;
    constexpr double step_tolerance = 1e-10;
    constexpr int max_damping_attempts = 20;

    LmResult result;
    result.x = std::move(initial);

    std::vector<double> residual = residual_function(result.x);
    if (static_cast<int>(residual.size()) != residual_rows) {
        throw std::runtime_error("LM 残差维数不正确");
    }

    double cost = squared_norm(residual);
    double damping = 1e-3;

    for (int iteration = 0; iteration < max_iterations; ++iteration) {
        const std::vector<double> jacobian = jacobian_function(result.x);
        if (static_cast<int>(jacobian.size()) !=
            residual_rows * parameter_columns) {
            throw std::runtime_error("LM 雅可比矩阵维数不正确");
        }

        std::vector<double> normal_matrix(
            static_cast<std::size_t>(parameter_columns * parameter_columns),
            0.0);
        std::vector<double> gradient(parameter_columns, 0.0);

        for (int i = 0; i < parameter_columns; ++i) {
            for (int j = 0; j < parameter_columns; ++j) {
                double value = 0.0;
                for (int row = 0; row < residual_rows; ++row) {
                    value += jacobian[row * parameter_columns + i] *
                             jacobian[row * parameter_columns + j];
                }
                normal_matrix[i * parameter_columns + j] = value;
            }
            for (int row = 0; row < residual_rows; ++row) {
                gradient[i] +=
                    jacobian[row * parameter_columns + i] * residual[row];
            }
        }

        bool accepted = false;
        double accepted_step_norm = 0.0;
        for (int attempt = 0; attempt < max_damping_attempts; ++attempt) {
            std::vector<double> damped_matrix = normal_matrix;
            for (int i = 0; i < parameter_columns; ++i) {
                // D 取法方程对角线，避免经纬度和高程量纲不同导致阻尼失衡。
                const double diagonal =
                    std::max(normal_matrix[i * parameter_columns + i], 1e-12);
                damped_matrix[i * parameter_columns + i] += damping * diagonal;
            }

            std::vector<double> right_hand_side = gradient;
            for (double& value : right_hand_side) {
                value = -value;
            }

            std::vector<double> step;
            if (!solve_linear_system(
                    std::move(damped_matrix),
                    std::move(right_hand_side),
                    step)) {
                damping = std::min(damping * 10.0, 1e12);
                continue;
            }

            std::vector<double> candidate = result.x;
            for (int i = 0; i < parameter_columns; ++i) {
                candidate[i] += step[i];
            }

            std::vector<double> candidate_residual =
                residual_function(candidate);
            const double candidate_cost = squared_norm(candidate_residual);
            if (std::isfinite(candidate_cost) && candidate_cost < cost) {
                result.x = std::move(candidate);
                residual = std::move(candidate_residual);
                cost = candidate_cost;
                damping = std::max(damping * 0.1, 1e-12);
                accepted = true;
                accepted_step_norm = vector_norm(step);
                break;
            }
            damping = std::min(damping * 10.0, 1e12);
        }

        result.iterations = iteration + 1;
        if (!accepted) {
            break;
        }

        const double rms =
            std::sqrt(cost / static_cast<double>(residual_rows));
        // 当前版本把“残差足够小”或“参数增量足够小”都视为数值收敛。
        // 因此 converged 表示 LM 已停止迭代，不单独保证定位精度很高。
        if (rms < residual_tolerance || accepted_step_norm < step_tolerance) {
            result.converged = true;
            break;
        }
    }

    const std::vector<double> final_jacobian = jacobian_function(result.x);
    result.residual_rms =
        std::sqrt(cost / static_cast<double>(residual_rows));
    result.condition_number =
        condition_number(final_jacobian, residual_rows, parameter_columns);
    return result;
}

PixelPoint forward_with_height_delta(
    const RpcModel& rpc,
    const std::vector<double>& x,
    int column,
    double delta) {
    // 数值求导辅助函数：只扰动当前列对应的参数。
    GeoPoint point{x[0], x[1], x[2]};
    if (column == 0) {
        point.longitude_deg += delta;
    } else if (column == 1) {
        point.latitude_deg += delta;
    } else {
        point.height_m += delta;
    }
    return rpc.forward(point);
}

}  // namespace

RfmSolution solve_fixed_height(
    const RpcModel& rpc,
    const PixelPoint& observed,
    double fixed_height_m) {
    // 未知量为经度、纬度；高程固定为用户提供的高程。
    const std::vector<double> initial = {
        rpc.longitude_offset(),
        rpc.latitude_offset(),
    };

    const auto residual_function = [&](const std::vector<double>& x) {
        const PixelPoint predicted =
            rpc.forward(GeoPoint{x[0], x[1], fixed_height_m});
        return std::vector<double>{
            predicted.sample - observed.sample,
            predicted.line - observed.line,
        };
    };

    const auto jacobian_function = [&](const std::vector<double>& x) {
        // 用前向差分近似偏导数，步长单位与经纬度一致（度）。
        constexpr double latitude_longitude_step = 1e-7;
        const PixelPoint base =
            rpc.forward(GeoPoint{x[0], x[1], fixed_height_m});

        std::vector<double> jacobian(2 * 2, 0.0);
        for (int column = 0; column < 2; ++column) {
            std::vector<double> perturbed = x;
            perturbed[column] += latitude_longitude_step;
            const PixelPoint shifted = rpc.forward(
                GeoPoint{perturbed[0], perturbed[1], fixed_height_m});
            jacobian[0 * 2 + column] =
                (shifted.sample - base.sample) / latitude_longitude_step;
            jacobian[1 * 2 + column] =
                (shifted.line - base.line) / latitude_longitude_step;
        }
        return jacobian;
    };

    const LmResult lm = solve_lm(
        initial,
        2,
        2,
        residual_function,
        jacobian_function);

    RfmSolution solution;
    solution.point = GeoPoint{lm.x[0], lm.x[1], fixed_height_m};
    solution.residual_rms_px = lm.residual_rms;
    solution.condition_number = lm.condition_number;
    solution.iterations = lm.iterations;
    solution.converged = lm.converged;
    return solution;
}

RfmSolution solve_two_image(
    const RpcModel& rpc_1,
    const RpcModel& rpc_2,
    const PixelPoint& observed_1,
    const PixelPoint& observed_2,
    const GeoPoint& initial) {
    // 未知量为经度、纬度、高程；两景影像各提供两个像点方程。
    const std::vector<double> initial_parameters = {
        initial.longitude_deg,
        initial.latitude_deg,
        initial.height_m,
    };

    const auto residual_function = [&](const std::vector<double>& x) {
        const GeoPoint point{x[0], x[1], x[2]};
        const PixelPoint predicted_1 = rpc_1.forward(point);
        const PixelPoint predicted_2 = rpc_2.forward(point);
        return std::vector<double>{
            predicted_1.sample - observed_1.sample,
            predicted_1.line - observed_1.line,
            predicted_2.sample - observed_2.sample,
            predicted_2.line - observed_2.line,
        };
    };

    const auto jacobian_function = [&](const std::vector<double>& x) {
        // 经度、纬度用角度步长，高程用米步长。
        constexpr double latitude_longitude_step = 1e-7;
        constexpr double height_step = 1e-3;
        const GeoPoint point{x[0], x[1], x[2]};
        const PixelPoint base_1 = rpc_1.forward(point);
        const PixelPoint base_2 = rpc_2.forward(point);

        std::vector<double> jacobian(4 * 3, 0.0);
        for (int column = 0; column < 3; ++column) {
            const double step =
                column == 2 ? height_step : latitude_longitude_step;
            const PixelPoint shifted_1 =
                forward_with_height_delta(rpc_1, x, column, step);
            const PixelPoint shifted_2 =
                forward_with_height_delta(rpc_2, x, column, step);

            jacobian[0 * 3 + column] =
                (shifted_1.sample - base_1.sample) / step;
            jacobian[1 * 3 + column] =
                (shifted_1.line - base_1.line) / step;
            jacobian[2 * 3 + column] =
                (shifted_2.sample - base_2.sample) / step;
            jacobian[3 * 3 + column] =
                (shifted_2.line - base_2.line) / step;
        }
        return jacobian;
    };

    const LmResult lm = solve_lm(
        initial_parameters,
        4,
        3,
        residual_function,
        jacobian_function);

    RfmSolution solution;
    solution.point = GeoPoint{lm.x[0], lm.x[1], lm.x[2]};
    solution.residual_rms_px = lm.residual_rms;
    solution.condition_number = lm.condition_number;
    solution.iterations = lm.iterations;
    solution.converged = lm.converged;
    return solution;
}

}  // namespace rpc_localization
