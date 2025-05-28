#include <map>
#include <string>

#include <curl/curl.h>

using string = std::string;

size_t curlCallback(void* contents, size_t size, size_t nmemb, void* userp);
string urlEncode(CURL* curl, const string& s);
string buildQuery(CURL* curl, const std::map<string, string>& params);