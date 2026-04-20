#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include "constants.h"

extern std::unordered_map<std::string, int> tickers;

void make_ticker(int n, char* out);
int ticker_index(const char* ticker);
void initialize_tickers();

