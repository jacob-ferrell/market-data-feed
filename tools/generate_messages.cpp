#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <unordered_map>

#include "../src/messages.h"

struct {
    std::uint64_t add;
    std::uint64_t cancel;
    std::uint64_t modify;
    std::uint64_t trade;
} order_counts;

static const char* TICKERS[] = {"AAPL", "MSFT", "GOOGL", "AMZN", "NVDA", "META", "TSLA", "JPM", "V", "UNH", "XOM", "JNJ", "WMT", "MA", "PG", "HD", "CVX", "MRK", "ABBV", "PEP"};

static const int TICKER_COUNT = 20;

static const int PRICE_POOL_SIZE = 8;

static const std::uint32_t PRICE_RANGE_LO = 100000;   // $10.00

static const std::uint32_t PRICE_RANGE_HI = 10000000; // $1000.00

static const std::uint32_t NUM_MESSAGES = 30000000;

static std::uint64_t sequence_num = 0;

static std::unordered_map<std::uint64_t, add_order_msg> live_orders = {};

static std::ofstream out_file("data/data.bin", std::ios::binary);

// 1. Obtain a random seed from hardware if available
static std::random_device rd;

// 2. Initialize the 32-bit Mersenne Twister engine with the seed
static std::mt19937 gen(rd());

// 3. Define a uniform distribution for the full uint32_t range
static std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);

static std::uint32_t ticker_prices[TICKER_COUNT][PRICE_POOL_SIZE];

void init_price_pools() {
    std::uniform_int_distribution<std::uint32_t> price_dis(PRICE_RANGE_LO, PRICE_RANGE_HI);
    for (int i = 0; i < TICKER_COUNT; i++) {
        for (int j = 0; j < PRICE_POOL_SIZE; j++) {
            ticker_prices[i][j] = price_dis(gen);
        }
    }
}

std::uint32_t price_for_ticker(int ticker_idx) {
    return ticker_prices[ticker_idx][dis(gen) % PRICE_POOL_SIZE];
}

void write_type_byte(MessageType type) {
    uint8_t t = static_cast<uint8_t>(type);
    out_file.write(reinterpret_cast<const char*>(&t), 1);
}

std::uint64_t get_order_id() {
    std::uint64_t id = sequence_num + 1;

    while (live_orders.count(id)) {
        id++;
    }

    return id;
}

uint64_t get_timestamp_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::uint32_t random_u32() {
    // Generate a random value
    return dis(gen);
}

int fill_ticker(char* dest) {
    int idx = random_u32() % TICKER_COUNT;
    const char* src = TICKERS[idx];
    memset(dest, '\0', 8);
    memcpy(dest, src, strlen(src));
    return idx;
}

add_order_msg create_add_order() {
    add_order_msg msg = {
        get_order_id(),
        sequence_num++,
        {},  // placeholder for ticker
        get_timestamp_ms(),
        0,   // placeholder for price
        random_u32(),
        random_u32() % 2 == 0 ? BID : ASK
    };

    int idx = fill_ticker(msg.ticker);
    msg.price = price_for_ticker(idx);

    live_orders.emplace(msg.order_id, msg);

    return msg;
}

cancel_order_msg create_cancel_order(std::uint64_t id) {
    cancel_order_msg msg = {
        id,
        sequence_num++,
        get_timestamp_ms(),
        {},  // placeholder for ticker
    };

    fill_ticker(msg.ticker);

    return msg;
}

modify_order_msg create_modify_order(std::uint64_t id) {
    modify_order_msg msg = {
        id,
        sequence_num++,
        get_timestamp_ms(),  // placeholder for ticker
        {},
        random_u32()
    };

    fill_ticker(msg.ticker);

    return msg;
}

trade_msg create_trade_order() {
    trade_msg msg = {
        sequence_num++,
        get_timestamp_ms(),
        {},  // placeholder for ticker
        random_u32(),
        0    // placeholder for price
    };

    int idx = fill_ticker(msg.ticker);
    msg.price = price_for_ticker(idx);

    return msg;
}

// 1. Specific function for ADD
void write_add_order() {
    add_order_msg msg = create_add_order();
    write_type_byte(ADD);
    out_file.write(reinterpret_cast<const char*>(&msg), sizeof(add_order_msg));
}

// 2. Specific function for CANCEL
void write_cancel_order() {
    auto it = live_orders.begin();
    if (it == live_orders.end()) return;

    std::uint64_t id = it->first;
    cancel_order_msg msg = create_cancel_order(id);

    live_orders.erase(it);  // Erase using iterator is faster than by key
    write_type_byte(CANCEL);

    out_file.write(reinterpret_cast<const char*>(&msg), sizeof(cancel_order_msg));
}

// 3. Specific function for MODIFY
void write_modify_order() {
    auto it = live_orders.begin();
    if (it == live_orders.end()) return;

    std::uint64_t id = it->first;
    // Note: If modify needs the ID from live_orders, pass it->first to
    // create_modify_order
    modify_order_msg msg = create_modify_order(id);
    write_type_byte(MODIFY);

    out_file.write(reinterpret_cast<const char*>(&msg), sizeof(modify_order_msg));
}

// 4. Specific function for TRADE
void write_trade_order() {
    trade_msg msg = create_trade_order();
    write_type_byte(TRADE);
    out_file.write(reinterpret_cast<const char*>(&msg), sizeof(trade_msg));
}

int main() {
    assert(out_file.is_open() && "File not open!");
    init_price_pools();

    // Weights: ADD=60, CANCEL=25, MODIFY=10, TRADE=5
    std::discrete_distribution<int> dist({60, 25, 10, 5});

    while (sequence_num < NUM_MESSAGES) {
        // Guard: if the book is empty we can only add
        int choice = live_orders.empty() ? 0 : dist(gen);

        switch (choice) {
            case 0:
                write_add_order();
                break;
            case 1:
                write_cancel_order();
                break;
            case 2:
                write_modify_order();
                break;
            case 3:
                write_trade_order();
                break;
        }
    }
    std::cout << "Generated " << sequence_num << " messages\n";
    std::cout << "  Live orders remaining: " << live_orders.size() << "\n";
    return 0;
}
