**How to use**
An example script can be seen in the examples folder. The following methods are implemented, covering all the functionality available on the Charles Schwab Developer API:

    string priceHistory(const std::map<string, string>& params);
    string optionChains(const std::map<string, string>& params);
    string optionExpirationChains(const string& symbol);
    string marketHours(const string& markets, const string& date);
    string movers(const string& indexSymbol, const string&sort, const int& frequency);
    string instruments(const string& symbol, const string& projection);
    string instruments(const string& cupid);
    string quotes(const string& symbols, const string& fields, const bool& indicative);
    string quotes(const string& symbol, const string& fields);

See the official Charles Schwab page for more information. This project is an unnoficial interface to simplify accessing the API in c++.