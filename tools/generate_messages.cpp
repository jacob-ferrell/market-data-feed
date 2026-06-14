#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <unordered_map>
#include <utility>
#include <vector>
#include "../src/constants.h"
#include "../src/messages.h"
#include "../src/ticker_registry.h"
#include "./timestamp_utils.h"
#include "../src/constants.h"

struct {
    std::uint64_t add;
    std::uint64_t cancel;
    std::uint64_t modify;
    std::uint64_t trade;
} order_counts;

static const std::uint32_t PRICE_RANGE_LO = 100000;   // $10.00
static const std::uint32_t PRICE_RANGE_HI = 10000000; // $1000.00

static std::uint64_t sequence_num = 0;
static std::uint64_t next_order_id = 0;

static std::unordered_map<std::uint64_t, add_order_msg> live_orders = {};

// Per-level order registry: maps price -> vector of order IDs, per instrument per side.
// Used to detect matches on add and to clean up on cancel.
static std::unordered_map<uint32_t, std::vector<uint64_t>> per_level[NUM_INSTRUMENTS][NUM_SIDES];

// Matched pairs waiting to be written as trade messages.
static std::vector<std::pair<uint64_t, uint64_t>> pending_trades; // (bid_id, ask_id)

static std::ofstream out_file("data/data.bin", std::ios::binary);

static std::random_device rd;
static std::mt19937 gen(rd());
static std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);

static std::string gen_tickers[NUM_INSTRUMENTS];
static std::uint32_t ticker_prices[NUM_INSTRUMENTS][MAX_LEVELS_PER_SIDE];

void init_tickers() {
    initialize_tickers();
    for (const auto& [ticker, idx] : tickers) {
        gen_tickers[idx] = ticker;
    }
}

void init_price_pools() {
    std::uniform_int_distribution<std::uint32_t> price_dis(PRICE_RANGE_LO, PRICE_RANGE_HI);
    for (int i = 0; i < NUM_INSTRUMENTS; i++) {
        for (int j = 0; j < MAX_LEVELS_PER_SIDE; j++) {
            ticker_prices[i][j] = price_dis(gen);
        }
    }
}

std::uint32_t price_for_ticker(int ticker_idx) {
    return ticker_prices[ticker_idx][dis(gen) % MAX_LEVELS_PER_SIDE];
}

void write_type_byte(MessageType type) {
    uint8_t t = static_cast<uint8_t>(type);
    out_file.write(reinterpret_cast<const char*>(&t), 1);
}

std::uint64_t get_order_id() {
    return ++next_order_id;
}
std::uint32_t random_u32() {
    return dis(gen);
}

int fill_ticker(char* dest) {
    int idx = random_u32() % NUM_INSTRUMENTS;
    memset(dest, '\0', 8);
    memcpy(dest, gen_tickers[idx].c_str(), gen_tickers[idx].size());
    return idx;
}

add_order_msg create_add_order() {
    add_order_msg msg = {
        get_order_id(),
        sequence_num++,
        {},
        get_timestamp_ms(),
        0,
        random_u32(),
        random_u32() % 2 == 0 ? BID : ASK
    };

    int idx = fill_ticker(msg.ticker);
    msg.price = price_for_ticker(idx);

    live_orders.emplace(msg.order_id, msg);
    return msg;
}

cancel_order_msg create_cancel_order(const add_order_msg& original) {
    cancel_order_msg msg = {
        original.order_id,
        sequence_num++,
        get_timestamp_ms(),
        {},
    };
    memcpy(msg.ticker, original.ticker, 8);
    return msg;
}

modify_order_msg create_modify_order(const add_order_msg& original) {
    modify_order_msg msg = {
        original.order_id,
        sequence_num++,
        get_timestamp_ms(),
        {},
        random_u32()
    };
    memcpy(msg.ticker, original.ticker, 8);
    return msg;
}


void write_add_order() {
    add_order_msg msg = create_add_order();
    int idx = ticker_index(msg.ticker);
    Side other_side = (msg.side == BID) ? ASK : BID;

    // Check for a matching order on the opposite side at the same price.
    auto& other = per_level[idx][other_side];
    auto match = other.find(msg.price);
    if (match != other.end() && !match->second.empty()) {
        uint64_t other_id = match->second.front();
        uint64_t bid_id = (msg.side == BID) ? msg.order_id : other_id;
        uint64_t ask_id = (msg.side == ASK) ? msg.order_id : other_id;
        pending_trades.emplace_back(bid_id, ask_id);
    }

    per_level[idx][msg.side][msg.price].push_back(msg.order_id);

    write_type_byte(ADD);
    out_file.write(reinterpret_cast<const char*>(&msg), sizeof(add_order_msg));
}

void write_cancel_order() {
    auto it = live_orders.begin();
    if (it == live_orders.end()) return;

    const add_order_msg& original = it->second;
    int idx = ticker_index(original.ticker);

    cancel_order_msg msg = create_cancel_order(original);

    auto& level_vec = per_level[idx][original.side][original.price];
    level_vec.erase(std::find(level_vec.begin(), level_vec.end(), original.order_id));
    if (level_vec.empty()) per_level[idx][original.side].erase(original.price);

    // Stale pending_trades entries referencing this order are handled lazily in write_trade_order.

    live_orders.erase(it);
    write_type_byte(CANCEL);
    out_file.write(reinterpret_cast<const char*>(&msg), sizeof(cancel_order_msg));
}

void write_modify_order() {
    auto it = live_orders.begin();
    if (it == live_orders.end()) return;

    add_order_msg& original = it->second;

    modify_order_msg msg = create_modify_order(original);

    original.quantity = msg.quantity;

    write_type_byte(MODIFY);
    out_file.write(reinterpret_cast<const char*>(&msg), sizeof(modify_order_msg));
}

void remove_from_level(int idx, Side side, uint32_t price, uint64_t order_id) {
    auto& level_vec = per_level[idx][side][price];
    level_vec.erase(std::find(level_vec.begin(), level_vec.end(), order_id));
    if (level_vec.empty()) per_level[idx][side].erase(price);
}

void write_trade_order() {
    while (!pending_trades.empty()) {
        auto [bid_id, ask_id] = pending_trades.back();
        pending_trades.pop_back();

        auto bid_it = live_orders.find(bid_id);
        auto ask_it = live_orders.find(ask_id);
        if (bid_it == live_orders.end() || ask_it == live_orders.end()) continue;

        const add_order_msg& bid = bid_it->second;
        const add_order_msg& ask = ask_it->second;

        uint32_t bid_qty = bid.quantity;
        uint32_t ask_qty = ask.quantity;
        uint32_t trade_qty = std::min(bid_qty, ask_qty);

        int idx = ticker_index(bid.ticker);

        trade_msg msg = {
            sequence_num++,
            get_timestamp_ms(),
            bid_id,
            ask_id,
            {}
        };
        memcpy(msg.ticker, bid.ticker, 8);

        if (bid_qty <= ask_qty) {
            remove_from_level(idx, BID, bid.price, bid_id);
            live_orders.erase(bid_it);
        } else {
            bid_it->second.quantity -= trade_qty;
        }

        if (ask_qty <= bid_qty) {
            remove_from_level(idx, ASK, ask.price, ask_id);
            live_orders.erase(ask_it);
        } else {
            ask_it->second.quantity -= trade_qty;
        }

        write_type_byte(TRADE);
        out_file.write(reinterpret_cast<const char*>(&msg), sizeof(trade_msg));
        return;
    }
    // No pending trades yet; fall back to an add order.
    write_add_order();
}

int main() {
    assert(out_file.is_open() && "File not open!");
    init_tickers();
    init_price_pools();

    // Weights: ADD=60, CANCEL=25, MODIFY=10, TRADE=5
    std::discrete_distribution<int> dist({60, 25, 10, 5});

    while (sequence_num < NUM_MESSAGES) {
        int choice = live_orders.empty() ? 0 : dist(gen);

        switch (choice) {
            case 0: write_add_order();    break;
            case 1: write_cancel_order(); break;
            case 2: write_modify_order(); break;
            case 3: write_trade_order();  break;
        }
    }

    std::cout << "Generated " << sequence_num << " messages\n";
    std::cout << "  Live orders remaining: " << live_orders.size() << "\n";
    return 0;
}
