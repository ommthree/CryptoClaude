#pragma once
#include <string>
#include <chrono>
#include <vector>

namespace CryptoClaude {
namespace Utils {

class DateTimeUtils {
public:
    // Conversion functions
    static std::chrono::system_clock::time_point fromUnixTime(int64_t unixTime);
    static int64_t toUnixTime(const std::chrono::system_clock::time_point& timePoint);

    static std::chrono::system_clock::time_point fromDateString(const std::string& dateStr, const std::string& format = "%Y-%m-%d");
    static std::string toDateString(const std::chrono::system_clock::time_point& timePoint, const std::string& format = "%Y-%m-%d");

    // API date format conversions
    static std::string toApiDateFormat(const std::chrono::system_clock::time_point& timePoint, const std::string& apiFormat = "%m%d%Y");
    static std::chrono::system_clock::time_point fromApiDateFormat(const std::string& apiDate, const std::string& apiFormat = "%m%d%Y");

    // Common date operations
    static std::chrono::system_clock::time_point addDays(const std::chrono::system_clock::time_point& timePoint, int days);
    static std::chrono::system_clock::time_point subtractDays(const std::chrono::system_clock::time_point& timePoint, int days);
    static int daysBetween(const std::chrono::system_clock::time_point& start, const std::chrono::system_clock::time_point& end);

    // Date range generation
    static std::vector<std::chrono::system_clock::time_point> generateDateRange(
        const std::chrono::system_clock::time_point& start,
        const std::chrono::system_clock::time_point& end,
        int dayStep = 1);

    static std::vector<std::string> generateDateStringRange(
        const std::string& startDate,
        const std::string& endDate,
        const std::string& format = "%Y-%m-%d",
        int dayStep = 1);

    // Day of week and month utilities
    static int getDayOfWeek(const std::chrono::system_clock::time_point& timePoint); // 1=Monday, 7=Sunday
    static int getMonthOfYear(const std::chrono::system_clock::time_point& timePoint); // 1=January, 12=December
    static std::string getDayOfWeekName(const std::chrono::system_clock::time_point& timePoint);
    static std::string getMonthName(const std::chrono::system_clock::time_point& timePoint);

    // Business day calculations
    static bool isWeekend(const std::chrono::system_clock::time_point& timePoint);
    static std::chrono::system_clock::time_point getNextBusinessDay(const std::chrono::system_clock::time_point& timePoint);
    static std::chrono::system_clock::time_point getPreviousBusinessDay(const std::chrono::system_clock::time_point& timePoint);

    // Time zone utilities (assuming UTC for crypto markets)
    static std::chrono::system_clock::time_point getCurrentTimeUTC();
    static std::string getCurrentDateStringUTC(const std::string& format = "%Y-%m-%d");

    // Validation
    static bool isValidDateString(const std::string& dateStr, const std::string& format = "%Y-%m-%d");
    static bool isValidTimeRange(const std::chrono::system_clock::time_point& start, const std::chrono::system_clock::time_point& end);

    // News API specific date mappings
    static std::string mapApiDateToSqlDate(const std::string& apiDate);
    static std::string mapSqlDateToApiDate(const std::string& sqlDate);

private:
    static std::tm timePointToTm(const std::chrono::system_clock::time_point& timePoint);
    static std::chrono::system_clock::time_point tmToTimePoint(const std::tm& tm);
};

} // namespace Utils
} // namespace CryptoClaude