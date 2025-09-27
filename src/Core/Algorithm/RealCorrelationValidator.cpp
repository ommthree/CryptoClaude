#include "RealCorrelationValidator.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <deque>
#include <fstream>
#include <iomanip>

namespace CryptoClaude {
namespace Algorithm {

RealCorrelationValidator::RealCorrelationValidator(const CorrelationConfig& config)
    : config_(config) {
}

RealCorrelationValidator::CorrelationResult
RealCorrelationValidator::calculateCorrelation(const TimeSeries& series1, const TimeSeries& series2) {
    CorrelationResult result;
    result.pair_name = series1.identifier + "/" + series2.identifier;
    result.calculation_time = std::chrono::system_clock::now();
    result.calculation_method = "real_data";

    // Align time series and extract valid paired values
    auto [aligned_series1, aligned_series2] = alignTimeSeries(series1, series2);

    if (config_.remove_outliers) {
        detectOutliers(aligned_series1);
        detectOutliers(aligned_series2);
    }

    auto [values1, values2] = extractAlignedValues(aligned_series1, aligned_series2);

    // Check minimum sample size requirement
    if (values1.size() < static_cast<size_t>(config_.minimum_sample_size)) {
        result.sample_size = values1.size();
        result.trs_status = "insufficient_data";
        return result;
    }

    result.sample_size = values1.size();
    result.valid_pairs = values1.size();
    result.data_completeness = static_cast<double>(result.valid_pairs) /
                              std::max(aligned_series1.data.size(), aligned_series2.data.size());

    // Calculate core correlation coefficients
    result.pearson_correlation = calculatePearsonCorrelation(values1, values2);
    result.spearman_correlation = calculateSpearmanCorrelation(values1, values2);
    result.kendall_tau = calculateKendallTau(values1, values2);

    // Calculate statistical significance
    result.degrees_of_freedom = result.sample_size - 2;
    result.p_value_pearson = calculatePearsonPValue(result.pearson_correlation, result.sample_size);
    result.p_value_spearman = calculateSpearmanPValue(result.spearman_correlation, result.sample_size);
    result.t_statistic = calculateTStatistic(result.pearson_correlation, result.sample_size);

    // Calculate confidence intervals
    auto [ci_lower, ci_upper] = calculateConfidenceInterval(result.pearson_correlation,
                                                           result.sample_size,
                                                           config_.confidence_level);
    result.confidence_interval_lower = ci_lower;
    result.confidence_interval_upper = ci_upper;
    result.confidence_level = config_.confidence_level;

    // Calculate rolling correlations if enabled
    if (config_.calculate_rolling && result.sample_size >= config_.rolling_window_size) {
        result.rolling_correlations = calculateRollingCorrelations(aligned_series1, aligned_series2);

        if (!result.rolling_correlations.empty()) {
            // Calculate correlation stability (standard deviation of rolling correlations)
            double mean_rolling = std::accumulate(result.rolling_correlations.begin(),
                                                result.rolling_correlations.end(), 0.0) /
                                result.rolling_correlations.size();

            double variance = 0.0;
            for (double corr : result.rolling_correlations) {
                variance += (corr - mean_rolling) * (corr - mean_rolling);
            }
            result.correlation_stability = std::sqrt(variance / result.rolling_correlations.size());

            // Calculate correlation trend
            result.correlation_trend = calculateLinearTrend(result.rolling_correlations);
        }
    }

    // TRS compliance assessment
    result.meets_trs_target = (result.pearson_correlation >= config_.trs_target_correlation);
    result.trs_gap = config_.trs_target_correlation - result.pearson_correlation;
    result.trs_status = determineTRSStatus(result.pearson_correlation, config_);

    // Set data time bounds
    if (!aligned_series1.data.empty()) {
        result.data_start_time = aligned_series1.data.front().timestamp;
        result.data_end_time = aligned_series1.data.back().timestamp;
    }

    return result;
}

RealCorrelationValidator::CorrelationResult
RealCorrelationValidator::calculateCorrelation(const std::vector<double>& data1,
                                             const std::vector<double>& data2,
                                             const std::string& pair_name) {
    // Convert vector data to TimeSeries format
    TimeSeries series1, series2;
    series1.identifier = pair_name + "_series1";
    series2.identifier = pair_name + "_series2";

    auto now = std::chrono::system_clock::now();

    for (size_t i = 0; i < data1.size(); ++i) {
        DataPoint point;
        point.timestamp = now + std::chrono::minutes(i);
        point.value = data1[i];
        point.is_valid = !std::isnan(data1[i]) && std::isfinite(data1[i]);
        series1.data.push_back(point);
    }

    for (size_t i = 0; i < data2.size(); ++i) {
        DataPoint point;
        point.timestamp = now + std::chrono::minutes(i);
        point.value = data2[i];
        point.is_valid = !std::isnan(data2[i]) && std::isfinite(data2[i]);
        series2.data.push_back(point);
    }

    auto result = calculateCorrelation(series1, series2);
    result.pair_name = pair_name;
    return result;
}

double RealCorrelationValidator::calculatePearsonCorrelation(const std::vector<double>& x,
                                                           const std::vector<double>& y) {
    if (x.size() != y.size() || x.empty()) {
        return 0.0;
    }

    size_t n = x.size();

    // Calculate means
    double mean_x = std::accumulate(x.begin(), x.end(), 0.0) / n;
    double mean_y = std::accumulate(y.begin(), y.end(), 0.0) / n;

    // Calculate covariance and variances
    double covariance = 0.0;
    double var_x = 0.0;
    double var_y = 0.0;

    for (size_t i = 0; i < n; ++i) {
        double diff_x = x[i] - mean_x;
        double diff_y = y[i] - mean_y;

        covariance += diff_x * diff_y;
        var_x += diff_x * diff_x;
        var_y += diff_y * diff_y;
    }

    // Calculate correlation coefficient
    if (var_x == 0.0 || var_y == 0.0) {
        return 0.0; // No variation in one or both variables
    }

    return covariance / std::sqrt(var_x * var_y);
}

double RealCorrelationValidator::calculateSpearmanCorrelation(const std::vector<double>& x,
                                                            const std::vector<double>& y) {
    if (x.size() != y.size() || x.empty()) {
        return 0.0;
    }

    // Convert to ranks
    auto ranks_x = calculateRanks(x);
    auto ranks_y = calculateRanks(y);

    // Calculate Pearson correlation on ranks
    return calculatePearsonCorrelation(ranks_x, ranks_y);
}

double RealCorrelationValidator::calculateKendallTau(const std::vector<double>& x,
                                                    const std::vector<double>& y) {
    if (x.size() != y.size() || x.empty()) {
        return 0.0;
    }

    size_t n = x.size();
    int concordant_pairs = 0;
    int discordant_pairs = 0;

    // Count concordant and discordant pairs
    for (size_t i = 0; i < n - 1; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            double sign_x = (x[j] > x[i]) ? 1.0 : ((x[j] < x[i]) ? -1.0 : 0.0);
            double sign_y = (y[j] > y[i]) ? 1.0 : ((y[j] < y[i]) ? -1.0 : 0.0);

            if (sign_x * sign_y > 0) {
                concordant_pairs++;
            } else if (sign_x * sign_y < 0) {
                discordant_pairs++;
            }
        }
    }

    int total_pairs = n * (n - 1) / 2;
    if (total_pairs == 0) {
        return 0.0;
    }

    return static_cast<double>(concordant_pairs - discordant_pairs) / total_pairs;
}

double RealCorrelationValidator::calculatePearsonPValue(double correlation, int sample_size) {
    if (sample_size <= 2) {
        return 1.0;
    }

    // Calculate t-statistic
    double t = correlation * std::sqrt((sample_size - 2) / (1 - correlation * correlation));

    // Approximate p-value using t-distribution
    // For simplicity, using a basic approximation
    double degrees_of_freedom = sample_size - 2;

    // Two-tailed test
    double abs_t = std::abs(t);

    // Approximate p-value calculation (simplified)
    if (abs_t > 6.0) {
        return 0.0001; // Very highly significant
    } else if (abs_t > 4.0) {
        return 0.001;  // Highly significant
    } else if (abs_t > 3.0) {
        return 0.01;   // Very significant
    } else if (abs_t > 2.0) {
        return 0.05;   // Significant
    } else if (abs_t > 1.5) {
        return 0.15;   // Marginally significant
    } else {
        return 0.5;    // Not significant
    }
}

double RealCorrelationValidator::calculateSpearmanPValue(double correlation, int sample_size) {
    // For Spearman correlation, the distribution is similar to Pearson for large n
    // Use the same approximation for simplicity
    return calculatePearsonPValue(correlation, sample_size);
}

double RealCorrelationValidator::calculateTStatistic(double correlation, int sample_size) {
    if (sample_size <= 2) {
        return 0.0;
    }

    return correlation * std::sqrt((sample_size - 2) / (1 - correlation * correlation));
}

std::pair<double, double>
RealCorrelationValidator::calculateConfidenceInterval(double correlation,
                                                    int sample_size,
                                                    double confidence_level) {
    if (sample_size <= 3) {
        return {-1.0, 1.0};
    }

    // Fisher's z-transformation
    double z = 0.5 * std::log((1 + correlation) / (1 - correlation));
    double se_z = 1.0 / std::sqrt(sample_size - 3);

    // Critical value for given confidence level (approximation)
    double alpha = 1.0 - confidence_level;
    double z_critical = 1.96; // For 95% confidence interval

    if (confidence_level >= 0.99) z_critical = 2.576;
    else if (confidence_level >= 0.95) z_critical = 1.96;
    else if (confidence_level >= 0.90) z_critical = 1.645;

    double z_lower = z - z_critical * se_z;
    double z_upper = z + z_critical * se_z;

    // Transform back to correlation scale
    double r_lower = (std::exp(2 * z_lower) - 1) / (std::exp(2 * z_lower) + 1);
    double r_upper = (std::exp(2 * z_upper) - 1) / (std::exp(2 * z_upper) + 1);

    return {r_lower, r_upper};
}

std::pair<RealCorrelationValidator::TimeSeries, RealCorrelationValidator::TimeSeries>
RealCorrelationValidator::alignTimeSeries(const TimeSeries& ts1, const TimeSeries& ts2) {
    TimeSeries aligned_ts1 = ts1;
    TimeSeries aligned_ts2 = ts2;

    // Sort both series by timestamp
    aligned_ts1.sortByTime();
    aligned_ts2.sortByTime();

    // Find common time range
    auto start_time = std::max(aligned_ts1.start_time, aligned_ts2.start_time);
    auto end_time = std::min(aligned_ts1.end_time, aligned_ts2.end_time);

    // Filter data to common time range and align timestamps
    std::vector<DataPoint> filtered_data1, filtered_data2;

    // Create maps for faster lookup
    std::map<std::chrono::system_clock::time_point, DataPoint> map1, map2;

    for (const auto& point : aligned_ts1.data) {
        if (point.timestamp >= start_time && point.timestamp <= end_time) {
            map1[point.timestamp] = point;
        }
    }

    for (const auto& point : aligned_ts2.data) {
        if (point.timestamp >= start_time && point.timestamp <= end_time) {
            map2[point.timestamp] = point;
        }
    }

    // Find common timestamps
    for (const auto& [timestamp, point1] : map1) {
        auto it2 = map2.find(timestamp);
        if (it2 != map2.end()) {
            filtered_data1.push_back(point1);
            filtered_data2.push_back(it2->second);
        }
    }

    aligned_ts1.data = filtered_data1;
    aligned_ts2.data = filtered_data2;

    return {aligned_ts1, aligned_ts2};
}

void RealCorrelationValidator::detectOutliers(TimeSeries& ts) {
    if (ts.data.size() < 10) {
        return; // Need sufficient data for outlier detection
    }

    std::vector<double> values;
    for (const auto& point : ts.data) {
        if (point.is_valid) {
            values.push_back(point.value);
        }
    }

    if (values.empty()) return;

    // Calculate mean and standard deviation
    double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    double variance = 0.0;

    for (double value : values) {
        variance += (value - mean) * (value - mean);
    }

    double std_dev = std::sqrt(variance / values.size());

    // Mark outliers using z-score method
    if (std_dev > 0) {
        for (auto& point : ts.data) {
            if (point.is_valid) {
                double z_score = std::abs((point.value - mean) / std_dev);
                if (z_score > config_.outlier_z_threshold) {
                    point.is_outlier = true;
                }
            }
        }
    }
}

std::pair<std::vector<double>, std::vector<double>>
RealCorrelationValidator::extractAlignedValues(const TimeSeries& ts1, const TimeSeries& ts2) {
    std::vector<double> values1, values2;

    size_t min_size = std::min(ts1.data.size(), ts2.data.size());

    for (size_t i = 0; i < min_size; ++i) {
        const auto& point1 = ts1.data[i];
        const auto& point2 = ts2.data[i];

        if (point1.is_valid && point2.is_valid && !point1.is_outlier && !point2.is_outlier) {
            values1.push_back(point1.value);
            values2.push_back(point2.value);
        }
    }

    return {values1, values2};
}

std::vector<double>
RealCorrelationValidator::calculateRollingCorrelations(const TimeSeries& ts1, const TimeSeries& ts2) {
    std::vector<double> rolling_correlations;

    if (ts1.data.size() < static_cast<size_t>(config_.rolling_window_size) ||
        ts2.data.size() < static_cast<size_t>(config_.rolling_window_size)) {
        return rolling_correlations;
    }

    size_t window_size = config_.rolling_window_size;
    size_t step_size = config_.rolling_step_size;

    for (size_t start = 0; start <= ts1.data.size() - window_size; start += step_size) {
        std::vector<double> window_values1, window_values2;

        for (size_t i = start; i < start + window_size; ++i) {
            const auto& point1 = ts1.data[i];
            const auto& point2 = ts2.data[i];

            if (point1.is_valid && point2.is_valid && !point1.is_outlier && !point2.is_outlier) {
                window_values1.push_back(point1.value);
                window_values2.push_back(point2.value);
            }
        }

        if (window_values1.size() >= 10) { // Minimum points for reliable correlation
            double correlation = calculatePearsonCorrelation(window_values1, window_values2);
            rolling_correlations.push_back(correlation);
        }
    }

    return rolling_correlations;
}

std::vector<double> RealCorrelationValidator::calculateRanks(const std::vector<double>& values) {
    std::vector<std::pair<double, size_t>> value_index_pairs;
    for (size_t i = 0; i < values.size(); ++i) {
        value_index_pairs.emplace_back(values[i], i);
    }

    // Sort by value
    std::sort(value_index_pairs.begin(), value_index_pairs.end());

    std::vector<double> ranks(values.size());

    // Assign ranks (handling ties by average rank)
    for (size_t i = 0; i < value_index_pairs.size(); ++i) {
        size_t original_index = value_index_pairs[i].second;

        // Handle ties
        size_t start = i;
        while (i + 1 < value_index_pairs.size() &&
               value_index_pairs[i].first == value_index_pairs[i + 1].first) {
            ++i;
        }

        double average_rank = (start + i + 2) / 2.0; // +2 because ranks are 1-based

        for (size_t j = start; j <= i; ++j) {
            ranks[value_index_pairs[j].second] = average_rank;
        }
    }

    return ranks;
}

double RealCorrelationValidator::calculateLinearTrend(const std::vector<double>& y_values) {
    if (y_values.size() < 2) {
        return 0.0;
    }

    size_t n = y_values.size();
    std::vector<double> x_values(n);
    std::iota(x_values.begin(), x_values.end(), 0.0);

    // Calculate means
    double mean_x = (n - 1) / 2.0;
    double mean_y = std::accumulate(y_values.begin(), y_values.end(), 0.0) / n;

    // Calculate slope using least squares
    double numerator = 0.0;
    double denominator = 0.0;

    for (size_t i = 0; i < n; ++i) {
        numerator += (x_values[i] - mean_x) * (y_values[i] - mean_y);
        denominator += (x_values[i] - mean_x) * (x_values[i] - mean_x);
    }

    return (denominator != 0.0) ? (numerator / denominator) : 0.0;
}

std::string RealCorrelationValidator::determineTRSStatus(double correlation,
                                                       const CorrelationConfig& config) {
    if (correlation >= config.trs_target_correlation) {
        return "compliant";
    } else if (correlation >= config.trs_warning_threshold) {
        return "warning";
    } else if (correlation >= config.trs_critical_threshold) {
        return "critical";
    } else {
        return "failed";
    }
}

// Static utility methods
double RealCorrelationValidator::calculateSimpleCorrelation(const std::vector<double>& x,
                                                          const std::vector<double>& y) {
    RealCorrelationValidator validator;
    return validator.calculatePearsonCorrelation(x, y);
}

bool RealCorrelationValidator::isCorrelationStatisticallySignificant(double correlation,
                                                                   int sample_size,
                                                                   double alpha) {
    RealCorrelationValidator validator;
    double p_value = validator.calculatePearsonPValue(correlation, sample_size);
    return p_value < alpha;
}

std::string RealCorrelationValidator::interpretCorrelationStrength(double correlation) {
    double abs_corr = std::abs(correlation);

    if (abs_corr >= 0.90) {
        return "Very Strong";
    } else if (abs_corr >= 0.70) {
        return "Strong";
    } else if (abs_corr >= 0.50) {
        return "Moderate";
    } else if (abs_corr >= 0.30) {
        return "Weak";
    } else {
        return "Very Weak";
    }
}

double RealCorrelationValidator::calculateCorrelationPower(int sample_size,
                                                         double effect_size,
                                                         double alpha) {
    // Simplified power calculation
    if (sample_size < 10 || effect_size < 0.1) {
        return 0.0;
    }

    // Basic approximation for power calculation
    double power = 1.0 - std::exp(-0.5 * sample_size * effect_size * effect_size);
    return std::min(power, 0.99);
}

void RealCorrelationValidator::updateConfiguration(const CorrelationConfig& new_config) {
    config_ = new_config;
}

} // namespace Algorithm
} // namespace CryptoClaude