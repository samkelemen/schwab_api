**Overview**

An engine for backtesting derivatives and stock trading strategies.

**How to use**

To use the simulation engine. A trading stradegy must be implemented which generates buy/sell orders and requests data from the engine to generate these buy/sell orders. Additionally, the market schemas on which to train the data must be specified.

**Project Structure**

The codebase has a modular structure. The core modules are (1.) the interface with the Charles Schwab API, (2.) the data pipeline, which stores, retrieves, and processes data in/from a PostgreSQL database, (3.) the actual simulation engine, which interfaces with the data pipeline and trading model, from which it receives buy/sell orders and gives back the data the model requires to generate buy/sell orders.
