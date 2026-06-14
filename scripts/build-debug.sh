#!/bin/bash
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug && cmake --build build-debug --target market_data_feed
