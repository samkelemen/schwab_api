// 1. Replace APP_KEY / APP_SECRET / CALLBACK with your Schwab API credentials.
// 2. Run:    make example1
// 3. Execute ./example1 and follow the on‑screen auth prompt the first time; a
//    tokens.json file will be written for subsequent runs.

#include "schwab_api.hpp"
#include <iostream>
#include <map>
#include <string>

int main()
{
    const std::string APP_KEY = "APP_KEY"; // Replace with your app key
    const std::string APP_SECRET = "APP_SECRET"; // Replace with you app secret
    const std::string CALLBACK = "CALL_BACK"; // Replace with your callback url 
    const std::string TOKENS_FILE = "tokens.json";
    const std::chrono::milliseconds TIMEOUT_MS = std::chrono::milliseconds{100000}; // Change this for shorter or longer timeout before giving up on requests

    Client client = Client(APP_KEY, APP_SECRET, CALLBACK, TOKENS_FILE, TIMEOUT_MS);

    std::string json = client.quotes("AAPL", "quote,fundamental");
    std::cout << "\nPrice history response:\n" << json << std::endl;

    while (true);

    return 0;
}
