#include <iostream>
#include <vector>
#include <cmath>

class LowPassFilter {
private:
    double alpha;      // 滤波系数
    double prev_output; // 上一次的输出值
    bool initialized;  // 是否已初始化

public:
    // 构造函数：通过截止频率和采样频率计算系数
    LowPassFilter(double cutoff_freq, double sample_freq) {
        double dt = 1.0 / sample_freq;
        double rc = 1.0 / (2 * M_PI * cutoff_freq);
        alpha = dt / (rc + dt);
        prev_output = 0.0;
        initialized = false;
    }

    // 构造函数：直接设置alpha系数
    LowPassFilter(double alpha_coef) : alpha(alpha_coef), prev_output(0.0), initialized(false) {}

    // 滤波函数
    double filter(double input) {
        if (!initialized) {
            prev_output = input;
            initialized = true;
        }

        double output = alpha * input + (1 - alpha) * prev_output;
        prev_output = output;
        return output;
    }

    // 重置滤波器状态
    void reset() {
        prev_output = 0.0;
        initialized = false;
    }

    // 批量滤波
    std::vector<double> filter(const std::vector<double>& inputs) {
        std::vector<double> outputs;
        outputs.reserve(inputs.size());

        for (double input : inputs) {
            outputs.push_back(filter(input));
        }

        return outputs;
    }

    // 获取当前系数
    double getAlpha() const { return alpha; }
};