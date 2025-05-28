// 1. Replace APP_KEY / APP_SECRET / CALLBACK with your Schwab API credentials.
// 2. Run:    make example1
// 3. Execute ./example1 and follow the on‑screen auth prompt the first time; a
//    tokens.json file will be written for subsequent runs.

#include "schwab_api.hpp"
#include "utils.hpp"

using namespace std;

int main() {
    Client client(
        "your-app-key",
        "your-app-secret",
        "http://localhost/callback",
        "tokens.json",
        chrono::milliseconds(5000)   // 5 s timeout
    );

    // Fetch Apple (AAPL) daily price history for the past year
    map<string,string> params = {
        {"symbol",        "AAPL"},
        {"periodType",    "year"},
        {"frequencyType", "daily"}
    };

    string json = client.priceHistory(params);
    cout << json << endl;
    return 0;
}