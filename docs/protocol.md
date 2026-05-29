# Wire Protocol - v1

Binary, little-endian, request/response over TCP. Versioned from the first
message. All multi-byte fields are LE; structs are packed (no implicit
padding) - readers must consume exactly `body_length` bytes.

## Header (8 bytes)

| Offset | Size | Field              | Notes                           |
|-------:|-----:|--------------------|---------------------------------|
| 0      | 1    | `protocol_version` | Currently `0x01`                |
| 1      | 1    | `msg_type`         | See table below                 |
| 2      | 2    | `body_length`      | Bytes in body (excludes header) |
| 4      | 4    | `sequence`         | Monotonic per direction         |

Max `body_length` = 4096. Receivers MUST drop the connection on version
mismatch or oversize body - both flagged as `Framer::Error`.

## Message types

| Code   | Name          | Direction        |
|--------|---------------|------------------|
| 0x01   | `Logon`       | Both             |
| 0x02   | `Logout`      | Both             |
| 0x03   | `Heartbeat`   | Both             |
| 0x10   | `NewOrder`    | Client → Server  |
| 0x11   | `CancelOrder` | Client → Server  |
| 0x20   | `OrderAck`    | Server → Client  |
| 0x21   | `OrderReject` | Server → Client  |
| 0x22   | `Fill`        | Server → Client  |
| 0x23   | `Cancelled`   | Server → Client  |

## Bodies

### Logon (0x01) - 4 bytes
| Offset | Size | Field       |
|-------:|-----:|-------------|
| 0      | 4    | `client_id` |

### Logout (0x02) / Heartbeat (0x03) - 0 bytes

### NewOrder (0x10) - 32 bytes
| Offset | Size | Field             | Notes                                |
|-------:|-----:|-------------------|--------------------------------------|
| 0      | 8    | `client_order_id` | Used as the engine's `OrderId` (v1)  |
| 8      | 4    | `instrument_id`   |                                      |
| 12     | 1    | `side`            | 0=Buy, 1=Sell                        |
| 13     | 1    | `order_type`      | 0=Limit, 1=Market                    |
| 14     | 1    | `tif`             | 0=GTC, 1=IOC, 2=FOK, 3=Day           |
| 15     | 1    | padding           | Must be 0                            |
| 16     | 8    | `price`           | `int64` ticks                        |
| 24     | 8    | `quantity`        |                                      |

### CancelOrder (0x11) - 12 bytes
| Offset | Size | Field             |
|-------:|-----:|-------------------|
| 0      | 8    | `client_order_id` |
| 8      | 4    | `instrument_id`   |

### OrderAck (0x20) - 16 bytes
| Offset | Size | Field             |
|-------:|-----:|-------------------|
| 0      | 8    | `client_order_id` |
| 8      | 8    | `server_order_id` |

### OrderReject (0x21) - 9 bytes
| Offset | Size | Field             |
|-------:|-----:|-------------------|
| 0      | 8    | `client_order_id` |
| 8      | 1    | `reason`          |

Reasons: `0`=Unknown, `1`=UnknownInstrument, `2`=InvalidQuantity,
`3`=InvalidPrice, `4`=InvalidTif, `5`=NotLoggedOn, `6`=DuplicateOrderId,
`7`=InsufficientLiquidity.

### Fill (0x22) - 33 bytes
| Offset | Size | Field             |
|-------:|-----:|-------------------|
| 0      | 8    | `client_order_id` |
| 8      | 8    | `server_order_id` |
| 16     | 8    | `price`           |
| 24     | 8    | `quantity`        |
| 32     | 1    | `post_status`     | `OrderStatus` underlying |

### Cancelled (0x23) - 8 bytes
| Offset | Size | Field             |
|-------:|-----:|-------------------|
| 0      | 8    | `client_order_id` |

## Session state machine

```
NotLoggedOn ──Logon──► Active ──Logout──► Closing
                          │
                          └── NewOrder / CancelOrder permitted only here
```

`NewOrder` / `CancelOrder` outside `Active` → `OrderReject{NotLoggedOn}`.

## Limitations

- `server_order_id` currently equals `client_order_id`; server-side
  renumbering is not implemented.
- No sequence-gap detection on either side. Clients should sanity-check
  monotonicity; servers ignore client sequence numbers.
- `ModifyOrder` is not supported. Cancel + new is the workaround.
- No market data messages.
