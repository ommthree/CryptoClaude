#pragma once
#include <string>
#include <chrono>
#include <stdexcept>
#include <optional>
#include <cmath>

namespace CryptoClaude {
namespace Database {
namespace Models {

class MarketData {
private:
    std::string symbol_;
    std::chrono::system_clock::time_point timestamp_;
    std::string date_;
    double close_;
    double volumeFrom_;
    double volumeTo_;
    double netInflow_;
    double excessInflow_;
    double hourlyInflow_;
    int dayOfWeek_;
    int monthOfYear_;

    // Sentiment data (when joined)
    int articleCount_ = 0;
    std::optional<double> averageSentiment_;

    // Technical indicators (calculated/cached)
    mutable std::optional<double> rsi_;
    mutable std::optional<double> macd_;
    mutable std::optional<double> bollingerPosition_;

    void validatePrice(double price, const std::string& fieldName) const {
        if (std::isnan(price) || std::isinf(price)) {
            throw std::invalid_argument(fieldName + " cannot be NaN or infinite");
        }
        if (price < 0) {
            throw std::invalid_argument(fieldName + " cannot be negative");
        }
    }

    void validateVolume(double volume, const std::string& fieldName) const {
        if (std::isnan(volume) || std::isinf(volume)) {
            throw std::invalid_argument(fieldName + " cannot be NaN or infinite");
        }
        if (volume < 0) {
            throw std::invalid_argument(fieldName + " cannot be negative");
        }
    }

public:
    MarketData() = default;

    MarketData(const std::string& sym,
               const std::chrono::system_clock::time_point& ts,
               double closePrice,
               double volFrom,
               double volTo)
        : symbol_(sym), timestamp_(ts), close_(closePrice),
          volumeFrom_(volFrom), volumeTo_(volTo),
          netInflow_(0.0), excessInflow_(0.0), hourlyInflow_(0.0),
          dayOfWeek_(0), monthOfYear_(0) {
        validateSymbol(sym);
        validatePrice(closePrice, "Close price");
        validateVolume(volFrom, "Volume from");
        validateVolume(volTo, "Volume to");
    }

    // Validation methods
    void validateSymbol(const std::string& sym) const {
        if (sym.empty()) {
            throw std::invalid_argument("Symbol cannot be empty");
        }
        if (sym.length() > 20) {
            throw std::invalid_argument("Symbol too long (max 20 characters)");
        }
    }

    // Getters
    const std::string& getSymbol() const { return symbol_; }
    const std::chrono::system_clock::time_point& getTimestamp() const { return timestamp_; }
    const std::string& getDate() const { return date_; }
    double getClose() const { return close_; }
    double getVolumeFrom() const { return volumeFrom_; }
    double getVolumeTo() const { return volumeTo_; }
    double getNetInflow() const { return netInflow_; }
    double getExcessInflow() const { return excessInflow_; }
    double getHourlyInflow() const { return hourlyInflow_; }
    int getDayOfWeek() const { return dayOfWeek_; }
    int getMonthOfYear() const { return monthOfYear_; }
    int getArticleCount() const { return articleCount_; }
    std::optional<double> getAverageSentiment() const { return averageSentiment_; }

    // Setters with validation
    void setSymbol(const std::string& sym) {
        validateSymbol(sym);
        symbol_ = sym;
    }

    void setTimestamp(const std::chrono::system_clock::time_point& ts) {
        timestamp_ = ts;
        // Convert timestamp to date string
        auto time_t = std::chrono::system_clock::to_time_t(ts);
        std::tm tm;
#ifdef _WIN32
        gmtime_s(&tm, &time_t);
#else
        tm = *std::gmtime(&time_t);
#endif
        char buffer[11];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm);
        date_ = std::string(buffer);
        dayOfWeek_ = tm.tm_wday;
        monthOfYear_ = tm.tm_mon + 1;
    }

    void setClose(double price) {
        validatePrice(price, "Close price");
        close_ = price;
        // Clear cached technical indicators when price changes
        clearTechnicalIndicators();
    }

    void setVolumeFrom(double volume) {
        validateVolume(volume, "Volume from");
        volumeFrom_ = volume;
    }

    void setVolumeTo(double volume) {
        validateVolume(volume, "Volume to");
        volumeTo_ = volume;
    }

    void setNetInflow(double inflow) {
        if (std::isnan(inflow) || std::isinf(inflow)) {
            throw std::invalid_argument("Net inflow cannot be NaN or infinite");
        }
        netInflow_ = inflow;
    }

    void setExcessInflow(double inflow) {
        if (std::isnan(inflow) || std::isinf(inflow)) {
            throw std::invalid_argument("Excess inflow cannot be NaN or infinite");
        }
        excessInflow_ = inflow;
    }

    void setHourlyInflow(double inflow) {
        if (std::isnan(inflow) || std::isinf(inflow)) {
            throw std::invalid_argument("Hourly inflow cannot be NaN or infinite");
        }
        hourlyInflow_ = inflow;
    }

    void setArticleCount(int count) {
        if (count < 0) {
            throw std::invalid_argument("Article count cannot be negative");
        }
        articleCount_ = count;
    }

    void setAverageSentiment(double sentiment) {
        if (std::isnan(sentiment) || std::isinf(sentiment)) {
            throw std::invalid_argument("Average sentiment cannot be NaN or infinite");
        }
        if (sentiment < -1.0 || sentiment > 1.0) {
            throw std::invalid_argument("Average sentiment must be between -1.0 and 1.0");
        }
        averageSentiment_ = sentiment;
    }

    void clearAverageSentiment() {
        averageSentiment_.reset();
    }

    // Technical indicators (cached, calculated elsewhere)
    std::optional<double> getRSI() const { return rsi_; }
    std::optional<double> getMACD() const { return macd_; }
    std::optional<double> getBollingerPosition() const { return bollingerPosition_; }

    void setRSI(double rsi) {
        if (rsi < 0.0 || rsi > 100.0) {
            throw std::invalid_argument("RSI must be between 0.0 and 100.0");
        }
        rsi_ = rsi;
    }

    void setMACD(double macd) { macd_ = macd; }
    void setBollingerPosition(double position) { bollingerPosition_ = position; }

    void clearTechnicalIndicators() const {
        rsi_.reset();
        macd_.reset();
        bollingerPosition_.reset();
    }

    // Business logic methods
    bool hasValidPrice() const {
        return close_ > 0 && !std::isnan(close_) && !std::isinf(close_);
    }

    bool hasValidVolume() const {
        return (volumeFrom_ >= 0 && volumeTo_ >= 0 &&
                !std::isnan(volumeFrom_) && !std::isinf(volumeFrom_) &&
                !std::isnan(volumeTo_) && !std::isinf(volumeTo_));
    }

    bool hasSentimentData() const {
        return articleCount_ > 0 && averageSentiment_.has_value();
    }

    // Calculate volume-based metrics
    double getVolumeRatio() const {
        if (volumeTo_ == 0) return 0.0;
        return volumeFrom_ / volumeTo_;
    }

    double getTotalVolume() const {
        return volumeFrom_ + volumeTo_;
    }

    // Compatibility methods for CoinExclusionEngine
    double getPrice() const {
        return close_;
    }

    double getVolume() const {
        return getTotalVolume();
    }

    // Estimated bid/ask from volume ratio (placeholder)
    double getBidPrice() const {
        return close_ * 0.999; // Approximate 0.1% spread
    }

    double getAskPrice() const {
        return close_ * 1.001; // Approximate 0.1% spread
    }
};

class MarketCapData {
private:
    std::string symbol_;
    double marketCap_;

public:
    MarketCapData() = default;
    MarketCapData(const std::string& sym, double cap) : symbol_(sym), marketCap_(cap) {
        validateSymbol(sym);
        validateMarketCap(cap);
    }

    void validateSymbol(const std::string& sym) const {
        if (sym.empty()) {
            throw std::invalid_argument("Symbol cannot be empty");
        }
    }

    void validateMarketCap(double cap) const {
        if (std::isnan(cap) || std::isinf(cap) || cap < 0) {
            throw std::invalid_argument("Market cap must be a valid positive number");
        }
    }

    const std::string& getSymbol() const { return symbol_; }
    double getMarketCap() const { return marketCap_; }

    void setSymbol(const std::string& sym) {
        validateSymbol(sym);
        symbol_ = sym;
    }

    void setMarketCap(double cap) {
        validateMarketCap(cap);
        marketCap_ = cap;
    }
};

class LiquidityLambda {
private:
    std::string symbol_;
    double lambda250_;
    double lambda250_500_;
    double rSquared250_;

public:
    LiquidityLambda() = default;
    LiquidityLambda(const std::string& sym, double l250, double l250_500, double r2)
        : symbol_(sym), lambda250_(l250), lambda250_500_(l250_500), rSquared250_(r2) {
        validateSymbol(sym);
        validateLambda(l250, "lambda250");
        validateLambda(l250_500, "lambda250_500");
        validateRSquared(r2);
    }

    void validateSymbol(const std::string& sym) const {
        if (sym.empty()) {
            throw std::invalid_argument("Symbol cannot be empty");
        }
    }

    void validateLambda(double lambda, const std::string& fieldName) const {
        if (std::isnan(lambda) || std::isinf(lambda)) {
            throw std::invalid_argument(fieldName + " cannot be NaN or infinite");
        }
    }

    void validateRSquared(double r2) const {
        if (std::isnan(r2) || std::isinf(r2)) {
            throw std::invalid_argument("R-squared cannot be NaN or infinite");
        }
        if (r2 < 0.0 || r2 > 1.0) {
            throw std::invalid_argument("R-squared must be between 0.0 and 1.0");
        }
    }

    const std::string& getSymbol() const { return symbol_; }
    double getLambda250() const { return lambda250_; }
    double getLambda250_500() const { return lambda250_500_; }
    double getRSquared250() const { return rSquared250_; }

    void setSymbol(const std::string& sym) {
        validateSymbol(sym);
        symbol_ = sym;
    }

    void setLambda250(double lambda) {
        validateLambda(lambda, "lambda250");
        lambda250_ = lambda;
    }

    void setLambda250_500(double lambda) {
        validateLambda(lambda, "lambda250_500");
        lambda250_500_ = lambda;
    }

    void setRSquared250(double r2) {
        validateRSquared(r2);
        rSquared250_ = r2;
    }

    // Business logic
    bool hasGoodFit() const {
        return rSquared250_ > 0.7; // Example threshold
    }

    double getLambdaDifference() const {
        return lambda250_500_ - lambda250_;
    }
};

} // namespace Models
} // namespace Database
} // namespace CryptoClaude