#!/bin/bash
cmake -B build-asan -DCMAKE_BUILD_TYPE=Sanitize && cmake --build build-asan --target market_data_feed
