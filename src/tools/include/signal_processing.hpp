#ifndef SIGNAL_PROCESSING_HPP
#define SIGNAL_PROCESSING_HPP

#include <vector>
#include <cmath>

inline  std::vector<std::vector<double>> resample(std::vector<std::vector<double>> data, float fs, float new_fs){

    std::vector<std::vector<double>> data_resampled;

    float resample_ratio = new_fs / fs;

    for (const auto& signal : data) {
        std::vector<double> resampled_signal;
        int new_length = static_cast<int>(std::round(signal.size() * resample_ratio));

        for (int i = 0; i < new_length; ++i) {
            float original_index = i / resample_ratio;

            int index_left = static_cast<int>(std::floor(original_index));
            int index_right = std::min(index_left + 1, static_cast<int>(signal.size() - 1));

            float t = original_index - index_left;
            double interpolated_value = (1 - t) * signal[index_left] + t * signal[index_right];

            resampled_signal.push_back(interpolated_value);
        }

        data_resampled.push_back(resampled_signal);
    }

    return data_resampled;
}



#endif