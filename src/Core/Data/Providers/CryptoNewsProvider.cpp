#include "CryptoNewsProvider.h"
#include "../../Json/JsonHelper.h"
#include <algorithm>
#include <regex>
#include <sstream>

namespace CryptoClaude {
namespace Data {
namespace Providers {

using json = CryptoClaude::Json::json;
using CryptoClaude::Json::JsonHelper;

CryptoNewsProvider::CryptoNewsProvider(std::shared_ptr<IHttpClient> httpClient,
                                     const std::string& apiKey)
    : NewsApiProvider(httpClient, apiKey, "https://newsapi.org/v2"),
      cryptoKeywords_{
          "bitcoin", "btc", "ethereum", "eth", "cryptocurrency", "crypto",
          "blockchain", "defi", "nft", "altcoin", "mining", "wallet",
          "exchange", "trading", "hodl", "moon", "bear market", "bull market"
      } {

    // Initialize crypto-specific coin mappings
    coinKeywords_["BTC"] = {"bitcoin", "btc"};
    coinKeywords_["ETH"] = {"ethereum", "eth", "ether"};
    coinKeywords_["ADA"] = {"cardano", "ada"};
    coinKeywords_["DOT"] = {"polkadot", "dot"};
    coinKeywords_["LINK"] = {"chainlink", "link"};
    coinKeywords_["UNI"] = {"uniswap", "uni"};
    coinKeywords_["AAVE"] = {"aave"};
    coinKeywords_["SUSHI"] = {"sushi", "sushiswap"};
}

CryptoNewsProvider::~CryptoNewsProvider() = default;

std::vector<CryptoNewsArticle> CryptoNewsProvider::getCryptoNews(const std::string& query,
                                                                int maxResults,
                                                                const std::string& language,
                                                                const std::string& sortBy) {
    // Enhance query with crypto-specific terms
    std::string enhancedQuery = enhanceQueryWithCryptoTerms(query);

    // Get news using parent class method
    auto generalNews = getNews(enhancedQuery, maxResults, language, sortBy);

    // Convert to crypto-specific articles
    std::vector<CryptoNewsArticle> cryptoArticles;
    for (const auto& article : generalNews) {
        CryptoNewsArticle cryptoArticle = convertToCryptoArticle(article);
        if (cryptoArticle.overallCryptoSentiment != 0.0 || !cryptoArticle.mentionedCoins.empty()) {
            cryptoArticles.push_back(cryptoArticle);
        }
    }

    return cryptoArticles;
}

std::vector<CryptoNewsArticle> CryptoNewsProvider::getSymbolNews(const std::string& symbol,
                                                               int maxResults,
                                                               const std::string& language) {
    // Build query for specific cryptocurrency
    std::string query = buildSymbolQuery(symbol);

    return getCryptoNews(query, maxResults, language, "publishedAt");
}

std::vector<CryptoNewsArticle> CryptoNewsProvider::getMultiSymbolNews(const std::vector<std::string>& symbols,
                                                                    int maxResults,
                                                                    const std::string& language) {
    std::vector<CryptoNewsArticle> allNews;

    // Get news for each symbol and combine
    for (const auto& symbol : symbols) {
        auto symbolNews = getSymbolNews(symbol, maxResults / static_cast<int>(symbols.size()), language);
        allNews.insert(allNews.end(), symbolNews.begin(), symbolNews.end());
    }

    // Sort by publication date (most recent first)
    std::sort(allNews.begin(), allNews.end(),
        [](const CryptoNewsArticle& a, const CryptoNewsArticle& b) {
            return a.publishedAt > b.publishedAt;
        });

    // Limit to maxResults
    if (allNews.size() > static_cast<size_t>(maxResults)) {
        allNews.resize(maxResults);
    }

    return allNews;
}

CryptoMarketSentiment CryptoNewsProvider::analyzeCryptoMarketSentiment(const std::vector<std::string>& symbols,
                                                                     const std::chrono::hours& timeWindow) {
    CryptoMarketSentiment sentiment;
    sentiment.analysisTime = std::chrono::system_clock::now();
    sentiment.timeWindow = timeWindow;
    sentiment.symbolsAnalyzed = symbols;

    std::vector<double> allSentiments;

    // Analyze sentiment for each symbol
    for (const auto& symbol : symbols) {
        auto symbolNews = getSymbolNews(symbol, 50); // Get up to 50 articles per symbol

        if (!symbolNews.empty()) {
            CryptoSymbolSentiment symbolSentiment;
            symbolSentiment.symbol = symbol;
            symbolSentiment.articlesAnalyzed = static_cast<int>(symbolNews.size());

            std::vector<double> symbolSentiments;
            double totalMarketImpact = 0.0;

            for (const auto& article : symbolNews) {
                // Check if article is within time window
                auto articleAge = std::chrono::duration_cast<std::chrono::hours>(
                    sentiment.analysisTime - article.publishedAt);

                if (articleAge <= timeWindow) {
                    if (article.coinSentimentScores.find(symbol) != article.coinSentimentScores.end()) {
                        double sentimentScore = article.coinSentimentScores.at(symbol);
                        symbolSentiments.push_back(sentimentScore);
                        allSentiments.push_back(sentimentScore);
                    } else if (article.overallCryptoSentiment != 0.0) {
                        symbolSentiments.push_back(article.overallCryptoSentiment);
                        allSentiments.push_back(article.overallCryptoSentiment);
                    }

                    totalMarketImpact += article.marketImpactScore;
                }
            }

            if (!symbolSentiments.empty()) {
                // Calculate symbol sentiment metrics
                symbolSentiment.averageSentiment = std::accumulate(symbolSentiments.begin(), symbolSentiments.end(), 0.0) / symbolSentiments.size();

                // Count positive/negative articles
                for (double sent : symbolSentiments) {
                    if (sent > 0.1) {
                        symbolSentiment.positiveArticles++;
                    } else if (sent < -0.1) {
                        symbolSentiment.negativeArticles++;
                    } else {
                        symbolSentiment.neutralArticles++;
                    }
                }

                symbolSentiment.averageMarketImpact = totalMarketImpact / symbolNews.size();
                symbolSentiment.confidenceLevel = calculateSentimentConfidence(symbolSentiments);

                sentiment.symbolSentiments[symbol] = symbolSentiment;
            }
        }
    }

    // Calculate overall market sentiment
    if (!allSentiments.empty()) {
        sentiment.overallSentiment = std::accumulate(allSentiments.begin(), allSentiments.end(), 0.0) / allSentiments.size();

        // Calculate market sentiment distribution
        for (double sent : allSentiments) {
            if (sent > 0.1) {
                sentiment.positivePercentage++;
            } else if (sent < -0.1) {
                sentiment.negativePercentage++;
            } else {
                sentiment.neutralPercentage++;
            }
        }

        int totalSentiments = static_cast<int>(allSentiments.size());
        sentiment.positivePercentage /= totalSentiments;
        sentiment.negativePercentage /= totalSentiments;
        sentiment.neutralPercentage /= totalSentiments;

        sentiment.confidenceScore = calculateSentimentConfidence(allSentiments);
    }

    return sentiment;
}

std::string CryptoNewsProvider::enhanceQueryWithCryptoTerms(const std::string& query) const {
    if (query.empty()) {
        return "cryptocurrency OR bitcoin OR ethereum OR blockchain";
    }

    // Add crypto context to the query
    return query + " AND (cryptocurrency OR bitcoin OR blockchain)";
}

std::string CryptoNewsProvider::buildSymbolQuery(const std::string& symbol) const {
    std::stringstream queryBuilder;

    // Add symbol itself
    queryBuilder << symbol;

    // Add known keywords for this symbol
    auto it = coinKeywords_.find(symbol);
    if (it != coinKeywords_.end()) {
        for (const auto& keyword : it->second) {
            queryBuilder << " OR " << keyword;
        }
    }

    // Add general crypto context
    queryBuilder << " AND cryptocurrency";

    return queryBuilder.str();
}

CryptoNewsArticle CryptoNewsProvider::convertToCryptoArticle(const NewsArticle& article) const {
    CryptoNewsArticle cryptoArticle;

    // Copy base article properties
    static_cast<NewsArticle&>(cryptoArticle) = article;

    // Analyze content for crypto-specific information
    std::string content = article.title + " " + article.description + " " + article.content;
    std::transform(content.begin(), content.end(), content.begin(), ::tolower);

    // Identify mentioned coins
    cryptoArticle.mentionedCoins = identifyMentionedCoins(content);

    // Calculate sentiment scores for each mentioned coin
    for (const auto& coin : cryptoArticle.mentionedCoins) {
        double sentimentScore = calculateCoinSentiment(content, coin);
        cryptoArticle.coinSentimentScores[coin] = sentimentScore;
    }

    // Calculate overall crypto sentiment
    cryptoArticle.overallCryptoSentiment = calculateOverallCryptoSentiment(content);

    // Calculate market impact score
    cryptoArticle.marketImpactScore = calculateMarketImpactScore(article, cryptoArticle);

    // Calculate source credibility
    cryptoArticle.sourceCredibility = calculateSourceCredibility(article.source);

    return cryptoArticle;
}

std::vector<std::string> CryptoNewsProvider::identifyMentionedCoins(const std::string& content) const {
    std::vector<std::string> mentionedCoins;

    // Check for each known cryptocurrency
    for (const auto& coinPair : coinKeywords_) {
        const std::string& symbol = coinPair.first;
        const std::vector<std::string>& keywords = coinPair.second;

        bool mentioned = false;
        for (const auto& keyword : keywords) {
            if (content.find(keyword) != std::string::npos) {
                mentioned = true;
                break;
            }
        }

        if (mentioned) {
            mentionedCoins.push_back(symbol);
        }
    }

    return mentionedCoins;
}

double CryptoNewsProvider::calculateCoinSentiment(const std::string& content, const std::string& coin) const {
    // Simplified sentiment analysis
    // In a real implementation, this would use NLP libraries

    // Positive keywords
    std::vector<std::string> positiveKeywords = {
        "rise", "bull", "bullish", "up", "gain", "profit", "moon", "surge",
        "rally", "breakthrough", "adoption", "partnership", "upgrade", "positive"
    };

    // Negative keywords
    std::vector<std::string> negativeKeywords = {
        "fall", "bear", "bearish", "down", "loss", "crash", "dump", "decline",
        "drop", "plummet", "hack", "ban", "regulation", "bubble", "negative"
    };

    int positiveCount = 0;
    int negativeCount = 0;

    // Count positive keywords
    for (const auto& keyword : positiveKeywords) {
        size_t pos = 0;
        while ((pos = content.find(keyword, pos)) != std::string::npos) {
            positiveCount++;
            pos += keyword.length();
        }
    }

    // Count negative keywords
    for (const auto& keyword : negativeKeywords) {
        size_t pos = 0;
        while ((pos = content.find(keyword, pos)) != std::string::npos) {
            negativeCount++;
            pos += keyword.length();
        }
    }

    // Calculate sentiment score (-1 to +1)
    int totalKeywords = positiveCount + negativeCount;
    if (totalKeywords == 0) {
        return 0.0; // Neutral
    }

    double sentiment = static_cast<double>(positiveCount - negativeCount) / totalKeywords;
    return std::max(-1.0, std::min(1.0, sentiment));
}

double CryptoNewsProvider::calculateOverallCryptoSentiment(const std::string& content) const {
    // Calculate sentiment for the overall crypto market mentioned in the content
    return calculateCoinSentiment(content, "crypto"); // Use general crypto sentiment
}

double CryptoNewsProvider::calculateMarketImpactScore(const NewsArticle& article,
                                                     const CryptoNewsArticle& cryptoArticle) const {
    double impact = 0.0;

    // Factor 1: Source credibility (higher credibility = higher impact)
    impact += cryptoArticle.sourceCredibility * 0.3;

    // Factor 2: Number of mentioned coins (more coins = broader impact)
    impact += std::min(1.0, cryptoArticle.mentionedCoins.size() / 5.0) * 0.2;

    // Factor 3: Article freshness (newer = higher impact)
    auto now = std::chrono::system_clock::now();
    auto articleAge = std::chrono::duration_cast<std::chrono::hours>(now - article.publishedAt);
    double freshnessScore = std::max(0.0, 1.0 - (articleAge.count() / 24.0)); // Decay over 24 hours
    impact += freshnessScore * 0.2;

    // Factor 4: Content length (longer articles might have more impact)
    double lengthScore = std::min(1.0, article.content.length() / 1000.0); // Normalize to 1000 characters
    impact += lengthScore * 0.1;

    // Factor 5: Absolute sentiment strength (stronger sentiment = higher impact)
    double sentimentStrength = std::abs(cryptoArticle.overallCryptoSentiment);
    impact += sentimentStrength * 0.2;

    return std::min(1.0, impact);
}

double CryptoNewsProvider::calculateSourceCredibility(const std::string& source) const {
    // Simplified source credibility scoring
    // In a real implementation, this would use a comprehensive database of source ratings

    // High credibility sources
    std::vector<std::string> highCredibilitySources = {
        "reuters", "bloomberg", "coindesk", "cointelegraph", "wall street journal",
        "financial times", "cnbc", "forbes", "business insider"
    };

    // Medium credibility sources
    std::vector<std::string> mediumCredibilitySources = {
        "yahoo", "marketwatch", "seeking alpha", "crypto news", "bitcoin.com",
        "decrypt", "the block", "coinmarketcap"
    };

    std::string lowerSource = source;
    std::transform(lowerSource.begin(), lowerSource.end(), lowerSource.begin(), ::tolower);

    // Check high credibility sources
    for (const auto& highSource : highCredibilitySources) {
        if (lowerSource.find(highSource) != std::string::npos) {
            return 0.9; // High credibility
        }
    }

    // Check medium credibility sources
    for (const auto& mediumSource : mediumCredibilitySources) {
        if (lowerSource.find(mediumSource) != std::string::npos) {
            return 0.7; // Medium credibility
        }
    }

    return 0.5; // Default/unknown credibility
}

double CryptoNewsProvider::calculateSentimentConfidence(const std::vector<double>& sentiments) const {
    if (sentiments.empty()) {
        return 0.0;
    }

    if (sentiments.size() == 1) {
        return 0.5; // Low confidence with single data point
    }

    // Calculate standard deviation (lower = higher confidence)
    double mean = std::accumulate(sentiments.begin(), sentiments.end(), 0.0) / sentiments.size();

    double variance = 0.0;
    for (double sentiment : sentiments) {
        variance += (sentiment - mean) * (sentiment - mean);
    }
    variance /= sentiments.size();
    double stdDev = std::sqrt(variance);

    // Convert to confidence score (0-1, where 1 is highest confidence)
    double confidence = std::max(0.0, 1.0 - (stdDev * 2.0)); // Scale standard deviation

    // Boost confidence if we have more data points
    double sampleSizeBonus = std::min(0.3, sentiments.size() / 50.0); // Max 0.3 bonus for 50+ samples
    confidence += sampleSizeBonus;

    return std::min(1.0, confidence);
}

} // namespace Providers
} // namespace Data
} // namespace CryptoClaude