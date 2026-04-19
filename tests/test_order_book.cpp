#include <cassert>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <unordered_map>
#include <unordered_set>
#include "../src/messages.h"
#include "../src/order_book.h"

static uint64_t seq = 1;

std::unordered_set<uint32_t> ticker_indexes;

static void make_ticker(int n, char* out) {
      out[0] = 'A' + (n / 676) % 26;
      out[1] = 'A' + (n / 26) % 26;
      out[2] = 'A' + n % 26;
      out[3] = '0' + n % 10;
      out[4] = '\0';
}

static message make_add(uint64_t order_id, uint32_t price, uint32_t qty, Side side, const char* instrument) {
    message msg;
    msg.type = ADD;
    msg.add.order_id = order_id;
    msg.add.sequence_num = seq++;
    memset(msg.add.ticker, '\0', 8);
    memcpy(msg.add.ticker, instrument, 4);
    msg.add.timestamp = 0;
    msg.add.price = price;
    msg.add.quantity = qty;
    msg.add.side = side;
    return msg;
}

static message make_cancel(uint64_t order_id, const char* instrument) {
    message msg;
    msg.type = CANCEL;
    msg.cancel.order_id = order_id;
    msg.cancel.sequence_num = seq++;
    msg.cancel.timestamp = 0;
    memset(msg.cancel.ticker, '\0', 8);
    memcpy(msg.cancel.ticker, instrument, 4);
    return msg;
}

static message make_modify(uint64_t order_id, uint32_t new_qty, const char* instrument) {
    message msg;
    msg.type = MODIFY;
    msg.modify.order_id = order_id;
    msg.modify.sequence_num = seq++;
    msg.modify.timestamp = 0;
    memset(msg.modify.ticker, '\0', 8);
    memcpy(msg.modify.ticker, instrument, 4);
    msg.modify.quantity = new_qty;
    return msg;
}

void test_instrument(const char* instrument) {
    // --- ADD ---
    // 4 BID price levels, 2 orders each, qty=1
    // Prices (descending): 980000, 970000, 960000, 950000  ($98, $97, $96, $95)
    // order_ids 1..8
    // Expected: each BID level qty=2
    uint32_t first_bid_order_id = seq;

    for (int i = 0; i < 4; i++) {
        uint32_t price = 980000 - i * 10000;
        for (int j = 0; j < 2; j++) {
            uint64_t order_id = seq;
            message msg = make_add(order_id, price, 1, BID, instrument);
            process_message(msg);
        }
    }

    // 4 ASK price levels, 2 orders each, qty=1
    // Prices (ascending): 1000000, 1010000, 1020000, 1030000  ($100, $101, $102, $103)
    // order_ids 9..16
    // Expected: each ASK level qty=2

    uint32_t modify_order_id;

    for (int i = 0; i < 4; i++) {
        uint32_t price = 1000000 + i * 10000;
        for (int j = 0; j < 2; j++) {
            uint64_t order_id = seq;

            if (i == 1 && j == 0) modify_order_id = order_id;

            message msg = make_add(order_id, price, 1, ASK, instrument);
            process_message(msg);
        }
    }

    const book_t& book = ticker_book(instrument);

    // After ADDs: each level has qty=2, sorted correctly
    assert(book[0][0].price == 980000 && book[0][0].quantity == 2);
    assert(book[0][1].price == 970000 && book[0][1].quantity == 2);
    assert(book[0][2].price == 960000 && book[0][2].quantity == 2);
    assert(book[0][3].price == 950000 && book[0][3].quantity == 2);
    assert(book[1][0].price == 1000000 && book[1][0].quantity == 2);
    assert(book[1][1].price == 1010000 && book[1][1].quantity == 2);
    assert(book[1][2].price == 1020000 && book[1][2].quantity == 2);
    assert(book[1][3].price == 1030000 && book[1][3].quantity == 2);

    // --- CANCEL ---
    // Cancel order 1 (BID at 980000, qty=1)
    // Expected: BID 980000 qty=1
    {
        message msg = make_cancel(first_bid_order_id, instrument);
        process_message(msg);
    }

    assert(book[0][0].price == 980000 && book[0][0].quantity == 1);

    // --- MODIFY ---
    // Modify order 11 (ASK at 1010000, qty 1->2)
    // Expected: ASK 1010000 qty=3  (was 2, delta=+1)
    {
        message msg = make_modify(modify_order_id, 2, instrument);
        process_message(msg);
    }

    assert(book[1][1].price == 1010000 && book[1][1].quantity == 3);

    // Final state:
    // BID: [980000:1, 970000:2, 960000:2, 950000:2]
    // ASK: [1000000:2, 1010000:3, 1020000:2, 1030000:2]
    assert(anomaly_counts.add == 0);
    assert(anomaly_counts.cancel == 0);
    assert(anomaly_counts.modify == 0);

    std::printf("All assertions passed\n");

}

int main() {

    for (int i = 0; i < NUM_INSTRUMENTS; i++) {

        char ticker[5];

        make_ticker(i, ticker);

        int idx = ticker_index(ticker);

        printf("i=%d ticker=%s idx=%d\n", i, ticker, idx);

        assert(ticker_indexes.insert(idx).second);

        test_instrument(ticker);
    }

    return 0;
}
