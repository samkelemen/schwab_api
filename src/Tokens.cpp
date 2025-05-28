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
//                                Tokens
//==============================================================================

/*---------------------------------------------*/
/*      Tokens Constructors and destructors    */
/*---------------------------------------------*/
Tokens::Tokens(const string appKey,
               const string appSecret,
               const string callbackUrl,
               const string tokensFile,
               bool autoRefresh
)   : appKey_{appKey},
      appSecret_{appSecret},
      callbackUrl_{callbackUrl},
      tokensFile_{tokensFile}
{
    loadFromFile(tokensFile_);

    auto now = Clock::now();
    if (now >= expiresAt_ || now >= refreshExpiresAt_) {
        createTokens();
        std::cout << "Successfully created authorization tokens!" << std::endl << std::flush;
    }
    else {
        std::cout << "Successfully reauthorized from saved tokens!" << std::endl << std::flush;
    }

    if (autoRefresh) {
        startBackgroundRefresh();
    } else {
        std::cerr << "Warning: Tokens will not be updated automatically.\n";
    }
}

Tokens::~Tokens() {
    stopBackgroundRefresh();
}

/*------------------------------*/
/*      Accessor methods        */
/*------------------------------*/
/*
 * Getter for accessToken_ member variable.
 */
string Tokens::accessToken() const {
    return accessToken_;
}

/*
 * Getter for refreshToken_ member variable.
 */
string Tokens::refreshToken() const {
    return refreshToken_;
}

/*-----------------------------------------------*/
/*      Token creation and refresh methods       */
/*-----------------------------------------------*/
/*
 * Starts autorefresh on a new thread using blocking I/O.
 * Will be switching to non-blocking in the future.
 */
void Tokens::startBackgroundRefresh() {
    running_ = true;
    refreshThread_ = std::thread(&Tokens::refreshLoop, this);
}

/*
 * Stops thread autorefreshing authentification tokens.
 */
void Tokens::stopBackgroundRefresh() {
    running_ = false;
    if (refreshThread_.joinable())
        refreshThread_.join();
}

/*
 * Uses blocking I/O to refresh tokens -- will add nonblocking in future
 */
void Tokens::refreshLoop() {
    while (running_) {
        auto now = Clock::now();
        if (now >= expiresAt_) {
            try {
                std::cout << "refreshing tokens" << std::endl << std::flush;
                refreshTokens();
                std::cout << "successfully refreshed tokens" << std::endl << std::flush;
            } catch (...) {
                std::cerr << "Failed to refresh tokens\n";
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(30));
    }
}

/*                                                                                                                                                                                                                                                                                                                                                                                        
 * Creates authentification tokens and writes them a file
 * to save the state.
 */
void Tokens::createTokens() {
    // Direct user to authorize URL
    auto authUrl = baseUrl_ + "oauth/authorize?client_id=" + appKey_
                 + "&redirect_uri=" + curl_easy_escape(nullptr, callbackUrl_.c_str(), 0);
    std::cout << "Visit this link to authorize:\n" << authUrl << "\n";
    std::cout << "Enter the full redirect URL: ";
    string redirectUrl;
    std::getline(std::cin, redirectUrl);

    // Extract code
    auto pos = redirectUrl.find("code=");
    auto code = redirectUrl.substr(pos + 5);
    if (auto amp = code.find('&')) code.resize(amp);

    // 1) Build the POST body with a true URL-escaped redirect URI
    char* encUri = curl_easy_escape(nullptr,
                                    callbackUrl_.c_str(),
                                    (int)callbackUrl_.size());
    std::string body = "grant_type=authorization_code"
                     + std::string("&code=") + code
                     + std::string("&redirect_uri=") + encUri;
    curl_free(encUri);

    // Initialize curl
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("curl init failed");

    // Tell libcurl to do Basic auth
    curl_easy_setopt(curl, CURLOPT_URL, (baseUrl_ + "oauth/token").c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(curl, CURLOPT_USERPWD,
                     (appKey_ + ":" + appSecret_).c_str());

    // Send as application/x-www-form-urlencoded
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());

    // Capture the response
    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode rc = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (rc != CURLE_OK)
        throw std::runtime_error(curl_easy_strerror(rc));

    // Now parse resp
    auto j = json::parse(response);

    accessToken_ = j.at("access_token").get<string>();
    refreshToken_ = j.at("refresh_token").get<string>();
    expiresAt_ = Clock::now() + accessTimeoutSeconds_;
    refreshExpiresAt_ = Clock::now() + refreshTimeoutHours_;
    
    writeToFile(tokensFile_);
}

/*                                                                                                                                                                                                                                                                                                                                                                                        
 * Refreshes the tokens from the current refresh token.
 */
void Tokens::refreshTokens() {
    // Build the POST body, URL-escaping the refresh token itself:
    char* encToken = curl_easy_escape(nullptr,
                                      refreshToken_.c_str(),
                                      (int)refreshToken_.size());
    std::string body = "grant_type=refresh_token&refresh_token=" + std::string(encToken);
    curl_free(encToken);

    // Init curl
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("curl init failed in refreshTokens");

    // Point at the token endpoint and set up Basic-auth properly
    curl_easy_setopt(curl, CURLOPT_URL,
                     (baseUrl_ + "oauth/token").c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(curl, CURLOPT_USERPWD,
                     (appKey_ + ":" + appSecret_).c_str());

    // Post the form and capture the response
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,    &response);

    CURLcode rc = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (rc != CURLE_OK) {
        throw std::runtime_error(std::string("curl perform failed: ")
                                 + curl_easy_strerror(rc));
    }

    // Parse and throw error if we still didn’t get tokens
    auto j = json::parse(response);
    if (!j.contains("access_token")) {
        std::cerr << "Refresh failed, response:\n" << response << "\n";
        throw std::runtime_error("Missing access_token in refresh response");
    }

    // Store new tokens + expirations
    accessToken_       = j["access_token"].get<string>();
    refreshToken_      = j["refresh_token"].get<string>();
    expiresAt_         = Clock::now() + accessTimeoutSeconds_;
    refreshExpiresAt_  = Clock::now() + refreshTimeoutHours_;

    std::cout << "Authorized and generated token!" << std::endl << std::flush;

    // Write back to your tokensFile_ (no argument)
    writeToFile(tokensFile_);
}

/*-----------------------------------------------------------*/
/*      Methods to save and start from previous state        */
/*-----------------------------------------------------------*/
/*
 * @brief Loads the acces token, refresh token, access token expiration
  * time (ms since epoch), and refresh expiration time (ms since
  * epoch) from a json file to restart from a saved state
  */
void Tokens::loadFromFile(string& path) {
    std::ifstream in(path);
    // On first run, there is no file to load from
    if (!in) {
        expiresAt_ = Clock::time_point::min();
        refreshExpiresAt_ = Clock::time_point::min();
        return;
    }
    // Load from previous state
    json savedAuthState; 
    in >> savedAuthState;
    accessToken_  = savedAuthState.value("access_token",  "");
    refreshToken_ = savedAuthState.value("refresh_token", "");

    auto accessExpiration = savedAuthState.value("access_token_expiration",  0LL);
    auto refreshExpiration = savedAuthState.value("refresh_token_expiration", 0LL);

    expiresAt_ = Clock::time_point{std::chrono::seconds{accessExpiration}};
    refreshExpiresAt_ = Clock::time_point{std::chrono::seconds{refreshExpiration}};
}

/*
 * @brief Writes the new access token, refresh token, access token
 * expiration time (ms since epoch), and refresh token 
 * exiration (ms since epoch) to a json file to save the state.
 */
void Tokens::writeToFile(string& path) {
    // Build the JSON with integer timestamps
    json output;
    output["access_token"] = accessToken_;
    output["refresh_token"] = refreshToken_;
    output["access_token_expiration"] = std::chrono::duration_cast<std::chrono::seconds>(
                                        expiresAt_.time_since_epoch()
                                    ).count();
    output["refresh_token_expiration"] = std::chrono::duration_cast<std::chrono::seconds>(
                                        refreshExpiresAt_.time_since_epoch()
                                    ).count();

    // Open the same file we loaded from
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Could not open " + path + " for writing");
    }

    // Pretty-print and flush
    out << std::setw(2) << output << "\n";
    out.flush();
    if (!out) {
        throw std::runtime_error("Failed to write tokens to " + path);
    }
}

/*----------------------------------*/
/*      HTTP request methods        */
/*----------------------------------*/
string Tokens::httpPost(
    const string& url,
    const string& postFields,
    const std::vector<string>& headers
) {
    CURL* curl = curl_easy_init();
    string response;
    if (!curl)
        throw std::runtime_error("Unable to init CURL");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    struct curl_slist* slist = nullptr;
    for (auto& h : headers) {
        slist = curl_slist_append(slist, h.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(slist);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK)
        throw std::runtime_error(curl_easy_strerror(res));

    return response;
}
