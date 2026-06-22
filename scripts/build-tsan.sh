#!/bin/bash
cmake -B build-tsan -DCMAKE_BUILD_TYPE=TSan && cmake --build build-tsan --target market_data_feed
