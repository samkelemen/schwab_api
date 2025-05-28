start of readme
# Schwab API C++ Client

A lightweight C++20 wrapper for the Charles Schwab Developer REST API, providing easy-to-use methods for fetching market data, option chains, quotes, and more.

---

## Features

- Authenticate via OAuth2 with automatic token refresh  
- Convert human-readable dates to Unix epoch (ms)  
- Fetch historical price data (OHLCV)  
- Retrieve option chains and expiration series  
- Query market hours and top movers  
- Search instrument details and quotes  
- Thread-safe helper utilities  

---

## Prerequisites

- C++20-compatible compiler (e.g., GCC 11+, Clang 13+)  
- **libcurl** development headers  
- CMake (optional) **or** GNU Make  

---

## Installation

Clone and build with **Make**:

~~~bash
git clone https://github.com/yourusername/schwab-cpp-client.git
cd schwab-cpp-client
make                          # build static library + examples
make install PREFIX=/usr/local   # optional system-wide install
~~~

Or use **CMake**:

~~~bash
cmake -B build -S .
cmake --build build
~~~

---

## Getting Started

### 1. Obtain API Credentials

1. Register an application at the Charles Schwab Developer Portal.  
2. Note your **App Key**, **App Secret**, and **Redirect URI**.  
3. Create a local `tokens.json` (or any filename you prefer) to store and refresh OAuth tokens.  

### 2. Include and Initialize

~~~cpp
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
~~~

---

## API Reference

### `Client` Class

#### Constructor

~~~cpp
Client(
    const string& appKey,
    const string& appSecret,
    const string& callbackUrl,
    const string& tokensFile,
    const chrono::milliseconds timeoutMs
);
~~~

#### Utilities

| Method | Description |
| ------ | ----------- |
| `long long datetimeToEpoch("dd-mm-yyyy HH:MM:SS")` | Convert to epoch ms |
| `long long dateToEpoch("dd-mm-yyyy")`              | Convert date (00:00:00) to epoch ms |
| `bool containsReqArgs(params, reqArgs)`            | Ensure required keys present |
| `bool validKeys(params, valKeys)`                  | Validate parameter names |

#### Data Endpoints (all return raw JSON `std::string`)

| Method | Purpose | Key Parameters |
| ------ | ------- | -------------- |
| `priceHistory(params)`            | OHLCV history | **Required** `symbol`; optional `periodType`, `frequencyType`, `period`, `frequency`, `startDate`, `endDate`, … |
| `optionChains(params)`            | Option chains | **Required** `symbol`; optional `contractType`, `strikeCount`, `strategy`, … |
| `optionExpirationChains(symbol)`  | Expiration dates | — |
| `marketHours(markets, date)`      | Market hours | `markets` = `equity`, `bond`, `option`, `future`, `forex`; `date` = YYYY-MM-DD or `TODAY` |
| `movers(indexSymbol, sort, frequency)` | Top movers | e.g. `$DJI`, sort by `VOLUME`, … |
| `instruments(symbol, projection)` | Instrument search | `projection` = `fundamental`, `symbol-search`, … |
| `instruments(cusip)`              | Instrument by CUSIP | — |
| `quotes(symbols, fields, indicative)` | Quotes list | `symbols` comma-separated, optional `fields`, `indicative` |
| `quotes(symbol, fields)`          | Single-symbol quotes | — |

---

## Contributing

1. Fork the repository  
2. Create a feature branch: `git checkout -b feature/YourFeature`  
3. Commit changes: `git commit -m "Add new feature"`  
4. Push branch: `git push origin feature/YourFeature`  
5. Open a Pull Request  

Please match the existing coding style and include tests when applicable.

---

## License

Distributed under the MIT License. See the **LICENSE** file for full text.
end of readme
