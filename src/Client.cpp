#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <curl/curl.h>

#include "schwab_api.hpp"
#include "utils.hpp"

using string = std::string;

//==============================================================================
//                                Client
//==============================================================================

/*------------------------------------------------------*/
/*      Client class constructors and destructors       */
/*------------------------------------------------------*/
Client::Client(
    const string appKey,
    const string appSecret,
    const string callbackUrl,
    const string tokensFile,
    const std::chrono::milliseconds timeoutMs
)   : timeoutMs_(timeoutMs),
    tokens_{appKey, appSecret, callbackUrl, tokensFile, true} // sets autoRefresh to true
{ }

Client::~Client() = default;

/*------------------------------*/
/*      Time conversions        */
/*------------------------------*/
/* 
 * Converts "dd-mm-yyyy HH:MM:SS" to milliseconds since Unix epoch 
 */
long long Client::datetimeToEpoch(const std::string& datetime) {
    std::istringstream in{datetime};
    std::tm tm{};  
    in >> std::get_time(&tm, "%d-%m-%Y %H:%M:%S");
    if (in.fail()) {
        throw std::runtime_error("Bad dateTime: " + datetime);
    }
    tm.tm_isdst = 0;    // ensure deterministic behavior

    return static_cast<long long>(timegm(&tm)) * 1000LL;
}

/* 
 * Converts "dd-mm-yyyy" to milliseconds since Unix epoch 
 */
long long Client::dateToEpoch(const std::string& date) {
    return datetimeToEpoch(date + " 00:00:00");
}

/*------------------------------------*/
/*      Request helper methods        */
/*------------------------------------*/
/* 
 * Checks params map contains required arguments 
 */
bool Client::containsReqArgs(const std::map<string, string>& params, const std::set<string>& reqArgs) {
    for (auto const& key : reqArgs) {
        if (params.find(key) == params.end()) {
            return false;
        }
    }
    return true;
}

/*
 * Checks keys in param map are valid 
 */
bool validKeys(const std::map<string, string>& params, const std::set<string>& valKeys) {
    // Check params for correct arg names
    for (auto const& [key, value] : params) {
        if (valKeys.find(key) == valKeys.end()) {
            std::cout << "Invalid key!: " << key << std::endl << std::flush;
            return false;
        }
    }
    return true;
}

/*
 * @brief Perfroms a get request, and reports any errors.
 */
string Client::httpGet(const string& fullUrl, CURL* curl) {
    // Response body buffer
    string body;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);

    // Auth header
    struct curl_slist* headers = nullptr;
    string auth = "Authorization: Bearer " + tokens_.accessToken();
    headers = curl_slist_append(headers, auth.c_str());
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    // Core options
    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS,
                     static_cast<long>(timeoutMs_.count()));
    //curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");  // UN-COMMENT FOR DEBUGGING PURPOSES

    std::cout << fullUrl << std::endl << std::flush;

    // Perform and check for timeout
    CURLcode rc = curl_easy_perform(curl);
    if (rc == CURLE_OPERATION_TIMEDOUT) {
        std::cerr << "Request timed out after "
                  << timeoutMs_.count() << "ms\n";
        // clean up before returning
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return "";  // or some sentinel
    }
    else if (rc != CURLE_OK) {
        // any other curl error
        std::ostringstream err;
        err << "curl_easy_perform() failed: "
            << curl_easy_strerror(rc);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        throw std::runtime_error(err.str());
    }

    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return body;
}


/*--------------------------*/
/*      Data requests       */
/*--------------------------*/
/*
 * @brief Get historical Open, High, Low, Close, and Volume for a given 
 * frequency (i.e. aggregation). Frequency available is dependent on 
 * periodType selected. The datetime format is in EPOCH milliseconds.
 * 
 * @param params: should be a map. Must include "symbol" as a key.
 * Additionally "periodType", "frequencyType", "period, "frequency",
 * "startDate", "endDate", "needExtendedHoursData", and "needPreviousClose"
 * are valid keys.
 */
string Client::priceHistory(const std::map<string, string>& params) {
    std::set<string> reqArgs = {"symbol"};
    std::set<string> argNames = {
        "symbol",
        "periodType",
        "frequencyType",
        "period",
        "frequency",
        "startDate",
        "endDate",
        "needExtendedHoursData",
        "needPreviousClose"
    };

    // Check params
    if (!validKeys(params, argNames) || 
            !containsReqArgs(params, reqArgs)) {
        std::cout << "Invalid params to priceHistory!" << std::endl << std::flush;
        return "";
    }

    // Initialize curl
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to init libcurl");
    }

    // Build the query and configure curl handle
    string response;
    string fullUrl = baseUrl_ + "marketdata/v1/pricehistory"
                    + buildQuery(curl, params);
    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());

    // Make the post request and return the response
    return httpGet(fullUrl, curl);
}

/*
 * @brief Get Option Chain including information on options contracts 
 * associated with each expiration.
 *
 * @param params: should be a map. Must include symbol as a key.
 * Additionally "contractType" (CALL, PUT, ALL), "strikeCount",
 * "includeUnderlyingQuote", "stradegy" (SINGLE, ANALYTICAL, COVERED,
 * VERTICAL, CALENDAR, STRANGLE, STRADDLE, BUTTERFLY, CONDOR,
 * DIAGONAL, COLLAR, ROLL), "interval", "strike", "range" (ITM, NTM, OTM),
 * "fromDate" (yyyy-mm-dd), "startDate" (yyyy-mm-dd), "volatility",
 * "underlyingPrice", "interestRate", "daysToExpiration", "exMonth" 
 * (JAN, FEB, MAR, APR, MAY, JUN, JUL, AUG, SEP, OCT, NOV, DEC, ALL),
 * "optionType", "entitlement" (PN, NP, PP)
*/
string Client::optionChains(const std::map<string, string>& params) {
    std::set<string> reqArgs = {"symbol"};
    std::set<string> argNames = {
        "symbol",
        "contractType",
        "strikeCount",
        "includeUnderlyingQuote",
        "strategy",
        "interval",
        "range",
        "fromDate",
        "startDate",
        "volatility",
        "underlyingPrice",
        "interestRate",
        "daysToExpiration",
        "expMonth",
        "optionType",
        "entitlement"
    };

    // Check params
    if (!validKeys(params, argNames) || 
            !containsReqArgs(params, reqArgs)) {
        std::cout << "Invalid params to validKeys!" << std::endl << std::flush;
        return "";
    }

    // Initialize curl
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to init libcurl");
    }

    // Build the query and configure curl handle
    string response;
    string fullUrl = baseUrl_ + "marketdata/v1/chains"
                    + buildQuery(curl, params);
    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());

    // Make the post request and return the response
    return httpGet(fullUrl, curl);
}

/*
 * @brief Get Option Expiration (Series) information for an optionable symbol. 
 * Does not include individual options contracts for the underlying.
 *
 * @param symbol
*/
string Client::optionExpirationChains(const string& symbol) {
    std::map<string, string> params = {{"symbol", symbol}};

    // Initialize curl
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to init libcurl");
    }

    // Build the query and configure curl handle
    string response;
    string fullUrl = baseUrl_ + "marketdata/v1/expirationchain"
                    + buildQuery(curl, params);
    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());

    // Make the post request and return the response
    return httpGet(fullUrl, curl);
}

/*
 * @brief Get Market Hours for dates in the future across different markets.
 *
 * @param market: the market for which to fetch hours. Valid values are "equity", 
 * "bond", "option", "future", and "forex".
 * @param date: the date for which to fetch hours (YYYY-MM-DD)
*/
string Client::marketHours(const string& markets, const string& date) {
    std::map<string, string> params = {{"markets", markets}};
    if (date != "TODAY") {
        params["date"] = date;
    }

    // Initialize curl
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to init libcurl");
    }

    // Build the query and configure curl handle
    string response;
    string fullUrl = baseUrl_ + "marketdata/v1/markets"
                    + buildQuery(curl, params);
    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());

    // Make the post request and return the response
    return httpGet(fullUrl, curl);
}

/*
 * @brief Get a list of top 10 securities movement for a specific index.
 *
 * @param indexSymbols: which index to select ($DJI, $COMPX, $SPX, NYSE, 
 * NASDAQ, OTCBB, INDEX_ALL, EQUITY_ALL, OPTION_ALL, OPTION_PUT, OPTION_CALL)
 * @param sort: the method by which to sort (VOLUME, TRADES, PERCENT_CHANGE_UP,
 * PERCENT_CHANGE_DOWN)
 * @param frequency: (0, 1, 5, 10, 30, 60)
 * */
string Client::movers( const string& indexSymbol, const string& sort, const int& frequency) {
    std::map<string, string> params;
    if (sort != "NONE") {
        params["sort"] = sort;
    }
    if (sort != "NONE") {
        params["frequency"] = frequency;
    }

    // Initialize curl
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to init libcurl");
    }

    // Build the query and configure curl handle
    string response;
    string fullUrl = baseUrl_ + "marketdata/v1/movers/" + indexSymbol
                    + buildQuery(curl, params);
    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());

    // Make the post request and return the response
    return httpGet(fullUrl, curl);
}

/*
 * @brief Get Instruments details by using different projections. 
 * Get more specific fundamental instrument data by using fundamental
 * as the projection.
 *
 * @param symbol: Ticker for the asset
 * @param projection: Fundamental projection gives fundamental data
 *      (symbol-search, symbol-regex, desc-search, desc-regex, 
 *      search, fundamental)
 */
string Client::instruments(const string& symbol, const string& projection) {
    std::map<string, string> params = {
        {"symbol", symbol},
        {"projection", projection}
    };

    // Initialize curl
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to init libcurl");
    }

    // Build the query and configure curl handle
    string response;
    string fullUrl = baseUrl_ + "marketdata/v1/instruments"
                    + buildQuery(curl, params);
    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());

    // Make the post request and return the response
    return httpGet(fullUrl, curl);
}

/*
 * @brief Get basic instrument details by cusip
 *
 * @param cupid
 * */
string Client::instruments(const string& cupid) {
    // Initialize curl
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to init libcurl");
    }

    // Build the query and configure curl handle
    string response;
    string fullUrl = baseUrl_ + "marketdata/v1/instruments/" + cupid;
    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());

    // Make the post request and return the response
    return httpGet(fullUrl, curl);
}

/*
 * @brief Get Quotes by list of symbols.
 * 
 * @param symbols - comma seperates list of symbol(s)
 * @param fields - comma seperated list of root nodes
 *      (quote, fundamental, extended, reference, regular, ALL)
 * @param indicative - return indicative quotes for symbol in addition to symbol
 *      (boolean)
 */
string Client::quotes(const string& symbols, const string& fields, const bool& indicative) {
    std::map<string, string> params = {
        {"symbols", symbols},
        {"indicative", std::to_string(indicative)}
    };
    if (fields != "ALL") {
        params["fields"] = fields;
    }
    // Initialize curl
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to init libcurl");
    }

    // Build the query and configure curl handle
    string response;
    string fullUrl = baseUrl_ + "marketdata/v1/quotes"
                    + buildQuery(curl, params);
    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());

    // Make the post request and return the response
    return httpGet(fullUrl, curl);
}

/*
 * @brief Get Quotes by list of symbols.
 * 
 * @param symbols - comma seperates list of symbol(s)
 * @param fields - comma seperated list of root nodes
 *      (quote, fundamental, extended, reference, regular, ALL)
 */
string Client::quotes(const string& symbol, const string& fields) {
    std::map<string, string> params;
    if (fields != "ALL") {
        params["fields"] = fields;
    }

    // Initialize curl
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to init libcurl");
    }

    // Build the query and configure curl handle
    string response;
    string fullUrl = baseUrl_ + "marketdata/v1/" + symbol + "/quotes"
                    + buildQuery(curl, params);
    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());

    // Make the post request and return the response
    return httpGet(fullUrl, curl);
}
