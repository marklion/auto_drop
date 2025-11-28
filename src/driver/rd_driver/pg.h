#include <vector>
#include <deque>
#include <cmath>
#include <stdexcept>

class RealTimeSGFilter {
private:
    std::deque<double> data_window_;  // 数据窗口
    std::vector<double> coefficients_; // SG滤波器系数
    size_t window_size_;               // 窗口大小（必须是奇数）
    int poly_order_;                   // 多项式阶数

public:
    /**
     * @brief 构造函数
     * @param window_size 窗口大小，必须是奇数且大于多项式阶数
     * @param poly_order 多项式阶数，通常2或3
     */
    RealTimeSGFilter(size_t window_size = 11, int poly_order = 2)
        : window_size_(window_size), poly_order_(poly_order) {

        // 参数验证
        if (window_size % 2 == 0) {
            throw std::invalid_argument("窗口大小必须是奇数");
        }
        if (poly_order >= static_cast<int>(window_size)) {
            throw std::invalid_argument("多项式阶数必须小于窗口大小");
        }

        // 预计算SG滤波器系数
        calculateCoefficients();
    }

    /**
     * @brief 处理实时输入数据
     * @param input 新输入的数据点
     * @return 平滑后的数据点。如果窗口未填满，返回原始值
     */
    double process(double input) {
        // 将新数据加入窗口
        data_window_.push_back(input);

        // 保持窗口大小
        if (data_window_.size() > window_size_) {
            data_window_.pop_front();
        }

        // 如果窗口未填满，返回原始值
        if (data_window_.size() < window_size_) {
            return input;
        }

        // 应用SG滤波器
        return applyFilter();
    }

    /**
     * @brief 重置滤波器状态（清空数据窗口）
     */
    void reset() {
        data_window_.clear();
    }

    /**
     * @brief 检查窗口是否已填满（是否已准备好进行滤波）
     */
    bool isReady() const {
        return data_window_.size() >= window_size_;
    }

private:
    /**
     * @brief 计算Savitzky-Golay滤波器系数
     */
    void calculateCoefficients() {
        int m = (window_size_ - 1) / 2;  // 半窗口大小

        // 构建设计矩阵A
        std::vector<std::vector<double>> A(window_size_,
                                         std::vector<double>(poly_order_ + 1));

        for (int i = -m; i <= m; ++i) {
            for (int j = 0; j <= poly_order_; ++j) {
                A[i + m][j] = std::pow(i, j);
            }
        }

        // 计算 A^T * A
        std::vector<std::vector<double>> AT_A(poly_order_ + 1,
                                            std::vector<double>(poly_order_ + 1, 0));

        for (int i = 0; i <= poly_order_; ++i) {
            for (int j = 0; j <= poly_order_; ++j) {
                for (int k = 0; k < window_size_; ++k) {
                    AT_A[i][j] += A[k][i] * A[k][j];
                }
            }
        }

        // 计算 (A^T * A)^-1 * A^T 的第一行（对应中心点的系数）
        coefficients_ = solveSystem(AT_A, extractColumn(A, m));
    }

    /**
     * @brief 从矩阵中提取一列
     */
    std::vector<double> extractColumn(const std::vector<std::vector<double>>& matrix,
                                    int col_index) {
        std::vector<double> column(matrix.size());
        for (size_t i = 0; i < matrix.size(); ++i) {
            column[i] = matrix[i][col_index];
        }
        return column;
    }

    /**
     * @brief 解线性方程组 (使用高斯消元法)
     */
    std::vector<double> solveSystem(const std::vector<std::vector<double>>& A,
                                  const std::vector<double>& b) {
        int n = A.size();
        std::vector<std::vector<double>> augmented(n, std::vector<double>(n + 1));
        std::vector<double> x(n);

        // 构建增广矩阵
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                augmented[i][j] = A[i][j];
            }
            augmented[i][n] = b[i];
        }

        // 前向消元
        for (int i = 0; i < n; ++i) {
            // 部分主元
            int max_row = i;
            for (int k = i + 1; k < n; ++k) {
                if (std::abs(augmented[k][i]) > std::abs(augmented[max_row][i])) {
                    max_row = k;
                }
            }
            std::swap(augmented[i], augmented[max_row]);

            // 消元
            for (int k = i + 1; k < n; ++k) {
                double factor = augmented[k][i] / augmented[i][i];
                for (int j = i; j <= n; ++j) {
                    augmented[k][j] -= factor * augmented[i][j];
                }
            }
        }

        // 回代
        for (int i = n - 1; i >= 0; --i) {
            x[i] = augmented[i][n];
            for (int j = i + 1; j < n; ++j) {
                x[i] -= augmented[i][j] * x[j];
            }
            x[i] /= augmented[i][i];
        }

        return x;
    }

    /**
     * @brief 应用SG滤波器
     */
    double applyFilter() {
        double result = 0.0;
        for (size_t i = 0; i < window_size_; ++i) {
            result += coefficients_[i] * data_window_[i];
        }
        return result;
    }
};
