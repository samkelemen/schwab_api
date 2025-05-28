#pragma once

#include <string>
#include <map>
#include <chrono>
#include <set>
#include <thread>
#include <atomic>
#include <fstream>
#include <iostream>
#include <vector>

#include <nlohmann/json.hpp>
#include <curl/curl.h>

using string = std::string;
using json = nlohmann::json;
using Clock = std::chrono::system_clock;

/*--------------------------------------------------------------*/
/*      A class to handle creating tokens to access the         */
/*      Charles Schwab API and refresh tokens automatically     */
/*--------------------------------------------------------------*/
class Tokens {
    public:
        Tokens(
            const string appKey,
            const string appSecret,
            const string callbackUrl,
            const string tokensFile,
            bool autoRefresh = true
        );

        ~Tokens();  // stops background thread

        // Accessor methods
        string accessToken() const;
        string refreshToken() const;

        // Forces immediate token creation / refresh
        void createTokens();
        void refreshTokens();

    private:
        // HTTP post helper
        string httpPost(
            const string& url,
            const string& postFields,
            const std::vector<string>& headers
        );

        // File I/O
        void loadFromFile(string& path);
        void writeToFile(string& path);

        // Timing
        void startBackgroundRefresh();
        void stopBackgroundRefresh();
        void refreshLoop();

        // members
        const string appKey_, appSecret_, callbackUrl_;
        const string baseUrl_ = "https://api.schwabapi.com/v1/";
        string tokensFile_;

        std::atomic<bool> running_{false};
        std::thread refreshThread_;

        // token data
        string accessToken_;
        string refreshToken_;
        Clock::time_point expiresAt_;
        Clock::time_point refreshExpiresAt_;

        // Timeouts
        std::chrono::seconds accessTimeoutSeconds_{30 * 60};
        std::chrono::hours refreshTimeoutHours_{7 * 24};
};

/*----------------------------------------------------------*/
/*      A class to access the Charles Schwab API and        */
/*      make data requests. Composes Tokens class to        */
/*      get tokens and maintain access to API               */
/*----------------------------------------------------------*/
class Client {
    public:
        Client(
            const string appKey,
            const string appSecret,
            const string callbackUrl,
            const string tokensFile,
            std::chrono::milliseconds timeoutMs
        );
        ~Client();

        long long datetimeToEpoch(const string& datetime);
        long long dateToEpoch(const string& date);

        string priceHistory(const std::map<string, string>& params);
        string optionChains(const std::map<string, string>& params);
        string optionExpirationChains(const string& symbol);
        string marketHours(const string& markets, const string& date);
        string movers(const string& indexSymbol, const string&sort, const int& frequency);
        string instruments(const string& symbol, const string& projection);
        string instruments(const string& cupid);
        string quotes(const string& symbols, const string& fields, const bool& indicative);
        string quotes(const string& symbol, const string& fields);
    private:
        std::chrono::milliseconds timeoutMs_;
        const string baseUrl_ = "https://api.schwabapi.com/";
        Tokens tokens_;
        
        bool valideKeys(const std::map<string, string>& params, const std::set<string>& valKeys);
        bool containsReqArgs(const std::map<string, string>& params, const std::set<string>& reqArgNames);
        string httpGet(const string& fullUrl, CURL* curl);
};