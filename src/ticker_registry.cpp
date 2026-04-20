#include "ticker_registry.h"
#include <string>
#include <unordered_map>

std::unordered_map<std::string, int> tickers{};

void make_ticker(int n, char* out) {
      out[0] = 'A' + (n / 676) % 26;
      out[1] = 'A' + (n / 26) % 26;
      out[2] = 'A' + n % 26;
      out[3] = '0' + n % 10;
      out[4] = '\0';
}

int ticker_index(const char* ticker) {
    return tickers.at(ticker);
}

void initialize_tickers() {
    for (int i = 0; i < NUM_INSTRUMENTS; i++) {
        char ticker[5];
        make_ticker(i, ticker);
        tickers.emplace(ticker, i);
    }
}


