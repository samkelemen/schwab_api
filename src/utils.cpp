#include <sstream>

#include <curl/curl.h>

#include "utils.hpp"

using string = std::string;

/*--------------------------------------------------------------*/
/*      Helper functions for the Client and Tokens classes      */
/*--------------------------------------------------------------*/
/*
 * libcurl callback to collect response data
 */
size_t curlCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

/*
 * Helper to URL-encode a string
 */
string urlEncode(CURL* curl, const string& s) {
    char* output = curl_easy_escape(curl, s.c_str(), (int)s.length());
    string encoded(output);
    curl_free(output);
    return encoded;
}

/*
 * Helper to build query from a map 
 */
string buildQuery(CURL* curl, const std::map<string, string>& params) {
    if (params.empty()) return "";
    std::ostringstream ss;
    ss << '?';
    bool first = true;
    for (auto& kv : params) {
        if (!first) ss << '&';
        first = false;
        ss << urlEncode(curl, kv.first)
           << '='
           << urlEncode(curl, kv.second);
    }
    return ss.str();
}