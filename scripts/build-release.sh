#!/bin/bash
cmake -B build-release -DCMAKE_BUILD_TYPE=Release && cmake --build build-release --target market_data_feed
