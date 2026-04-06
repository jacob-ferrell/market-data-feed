# Binary Message Format

All messages are fixed-size, tightly packed binary structs with no padding.
Use `__attribute__((packed))` or `#pragma pack` to enforce this.

## Common Header (8 bytes)

| Field          | Type     | Size | Description                        |
|----------------|----------|------|------------------------------------|
| sequence_num   | uint32_t | 4    | Monotonically increasing per feed  |
| msg_type       | uint8_t  | 1    | See message types below            |
| instrument_id  | uint16_t | 2    | 0–999                              |
| _reserved      | uint8_t  | 1    | Must be zero                       |

## Message Types

| Type | Value | Description     |
|------|-------|-----------------|
| ADD  | 0x01  | Add order       |
| MOD  | 0x02  | Modify order    |
| CXL  | 0x03  | Cancel order    |
| TRD  | 0x04  | Trade executed  |

## ADD / MOD Payload (16 bytes after header)

| Field      | Type     | Size | Description                        |
|------------|----------|------|------------------------------------|
| order_id   | uint64_t | 8    | Unique order identifier            |
| price      | int64_t  | 8    | Price in fixed-point (price * 100) |
| quantity   | int32_t  | 4    | Number of shares/contracts         |
| side       | uint8_t  | 1    | 0 = Bid, 1 = Ask                  |
| _reserved  | uint8_t  | 3    | Must be zero                       |

## CXL Payload (8 bytes after header)

| Field     | Type     | Size | Description        |
|-----------|----------|------|--------------------|
| order_id  | uint64_t | 8    | Order to cancel    |

## TRD Payload (16 bytes after header)

| Field      | Type     | Size | Description                        |
|------------|----------|------|------------------------------------|
| price      | int64_t  | 8    | Trade price in fixed-point         |
| quantity   | int32_t  | 4    | Traded quantity                    |
| _reserved  | uint8_t  | 4    | Must be zero                       |

## Notes

- All integers are **little-endian**
- Prices use **fixed-point with 2 decimal places**: price 10050 = $100.50
- A **sequence gap** (sequence_num jumps by more than 1) indicates dropped messages — define your recovery policy before M1
