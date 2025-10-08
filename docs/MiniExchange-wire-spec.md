
# MiniExchange Wire Spec v1.0

**Transport:** TCP (TLS) — reliable ordered byte stream.

**Framing:** fixed 8-byte header + `payload_len` bytes. Read header first, then exact payload. Reject if `payload_len` > configured server limit (e.g. 1 MB).

**Endianness:** **big-endian** (network byte order).

**Identifiers:** UUIDs are 16 bytes RFC 4122 binary (not strings).

**Timestamps:** `int64_t` microseconds since Unix epoch (signed 8 bytes).

**Price/Qty:** integers (ticks and integer qty). Price = `int64_t` ticks. Qty = `int64_t` units (signed but validated non-negative for new orders).

**Server IDs:** server assigned IDs are `uint64_t` (8 bytes) used in internal references and notifications.

**Message types:** `uint8_t` in header.

---
# Primitive sizes (wire)

- `uint8_t`: 1 byte
- `uint16_t`: 2 bytes (BE)
- `uint32_t`: 4 bytes (BE)
- `uint64_t`: 8 bytes (BE)
- `int64_t`: 8 bytes (BE)
- `Timestamp`: `uint64_t` us since epoch (BE)
- `uint8_t[n]`: Array of `n` bytes


# Header (16 bytes)

All messages start with this 16 byte header:

- Message Type: `uint8_t` (see [table](#message-types) for all the message codes)
- Protocol version flag: `uint8_t` [`0x01`=v1.0]
- Reserved flags: `uint8_t[2]`
- Payload Length (bytes): `uint16_t`
- Client Message Sequence Number: `uint32_t`
- Server Message Sequence Number: `uint32_t`
- Reserved/Padding: `uint8_t[2]`


Strings: length-prefixed only in TLV or small info fields (`uint16_t len` + bytes).

# Message Types

Message Type | Message Code | Direction | Layout [header][payload][HMAC] | Message Size (bytes)|
---          | ---          | ---       | ---                            |                 --- | 
HELLO | 1  
HELLO_ACK | 2  
HEARTBEAT | 3 
LOGOUT | 4  |
LOGOUT_ACK | 5  
SESSION_TIMEOUT | 6 
NEW_ORDER | 10  
ORDER_ACK | 11 
CANCEL_ORDER | 12  
CANCEL_ACK | 13  |
MODIFY_ORDER | 14  
MODIFY_ACK | 15  |
TRADE | 20  
BOOK_SNAPSHOT_REQUEST | 30  
BOOK_SNAPSHOT | 31  
RESEND_REQUEST | 40  
RESEND_RESPONSE | 41  
ERROR | 100 


Protocol version | Header flag 
--- | --- |
v1.0 | 1


| Message Type                | Code (dec/hex) | Direction       | Payload Length     | Payload Layout                                                                                                                                                                             | Message Size |
| --------------------------- | -------------- | --------------- | ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------ |
| **HELLO**                   | 1 / `0x01`     | client → server | 48      | API Key: `uint8_t[16]` <br> HMAC: `uint8_t[32]`                                                         | 64           |
| **HELLO_ACK**              | 2 / `0x02`     | server -> client| 48       | Server Client ID `uint64_t` <br> Status: `uint8_t` <br> Padding: `uint8_t[7]` <br> HMAC: `uint8_t[32]`                                                       | 64           |
| **HEARTBEAT**               | 3 / `0x03`     | client -> server| 48      | Server Client ID `uint64_t` <br> Padding `uint8_t[8]` <br> HMAC: `uint8_t[32]`                                                                                                    | 64           |
| **LOGOUT**                  | 4 / `0x04`     | client -> server| 48      | Server Client ID `uint64_t` <br> Padding `uint8_t[8]`<br> HMAC: `uint8_t[32]`                                                                                                              | 64           |
| **LOGOUT_ACK**             | 5 / `0x05`     | server -> client| 48       | Server Client ID `uint64_t` <br> Status `uint8_t` <br> Padding `uint8_t[7]` <br> HMAC: `uint8_t[32]`                                                                                                                                                           | 64           |
| **SESSION_TIMEOUT**        | 6 / `0x06`     | server -> client| 48       | Server Client ID `uint64_t` <br> Server Time `uint64_t` <br> HMAC: `uint8_t[32]`                                                                                                                                                           | 64           |
| **NEW_ORDER**              | 10 / `0x0A`    | client -> server | 80      | Server Client ID `uint64_t` <br> Instrument ID `uint32_t` <br> Order Side `uint8_t` <br> Order Tyoe `uint8_t` <br> Quantity `int64_t` <br> Price `int64_t` <br> Time-in-Force `uint8_t` <br> Good-till Date `uint64_t` <br> Padding `uint8_t[9]` <br> HMAC `uint8_t[32]`| 96           |
| **ORDER_ACK**              | 11 / `0x0B`    | server - client | 80       | Server Client ID `uint64_t` <br> Instrument ID `uint32_t` <br>Server Order ID `uint64_t` <br> Status `uint8_t` <br> Accepted Price `int64_t` <br> Accepted Quantity `int64_t` <br> Server Time `uint64_t` <br>  Padding `uint8_t[3]`<br> HMAC `uint8_t[32]`| 96           |
| **CANCEL_ORDER**           | 12 / `0x0C`    | client - server | 64       | Server Client ID `uint64_t`<br> Server Order ID: `uint64_t` <br> Padding `uint8_t[16]` <br> HMAC: `uint8_t[32]`                                                                                                           | 80           |
| **CANCEL_ACK**             | 13 / `0x0D`    | server - client | 64       | Server Client ID `uint64_t` <br> Server Order ID: `uint64_t` <br> Status: `uint8_t` <br> Padding: `uint8_t[15]` <br> HMAC: `uint8_t[32]`                            | 80           |
| **MODIFY_ORDER**           | 14 / `0x0E`    | client - server | 64       | Server Client ID `uint64_t` <br>Server Order ID: `uint64_t` <br> New Quantity: `int64_t` <br> New Price `uint64_t` <br> HMAC: `uint8_t[32]`                                                    | 80           |
| **MODIFY_ACK**             | 15 / `0x0F`    | server - client | 80       | Server Client ID `uint64_t` <br> Old Server Order ID: `uint64_t` <br> New Server Order ID: `uint64_t` <br> New Quantity: `int64_t` <br> New Price `int64_t` <br> Status: `uint8_t` <br> Padding: `uint8_t[7]` <br> HMAC: `uint8_t[32]`                             | 96           |
| **TRADE**                   | 20 / `0x14`    | server - client | 80      | Server Client ID: `uint64_t` <br> Trade ID: `uint64_t` <br> Server Order ID: `uint64_t` <br> Filled Qty: `int64_t` <br> Filled Price: `int64_t` <br> Timestamp (us): `uint64_t` <br> HMAC: `uint8_t[32]` | 96           |
| **RESEND_REQUEST**         | 40 / `0x28`    | both ways       | 48       | Start Seq: `uint32_t` <br> End Seq: `uint32_t` <br> Padding: `uint8_t[8]` <br> HMAC: `uint8_t[32]`                                                                                         | 64           |
| **RESEND_RESPONSE**        | 41 / `0x29`    | both ways       | variable | Replayed messages (same format as originals)                                                                                                                                               | variable     |
| **ERROR**                   | 100 / `0x64`   | both ways       | 48      | Error Code: `uint16_t` <br> Padding: `uint8_t[14]` <br> HMAC: `uint8_t[32]`                                                                                                                | 64           |




| **BOOK_SNAPSHOT_REQUEST** | 30 / `0x1E`    | client - server | 32        | Instrument ID: `uint32_t` <br> Padding: `uint8_t[28]` <br> HMAC: `uint8_t[32]`                                                                                                             | 48           |
| **BOOK_SNAPSHOT**          | 31 / `0x1F`    | server - client | variable | Instrument ID: `uint32_t` <br> Levels: repeated (price `int64_t`, qty `int64_t`) <br> Padding as needed <br> HMAC: `uint8_t[32]`                                                           | variable     |


## Status Codes

Hello Ack:

0x01=OK, authentication accepted, user now logged in

0x02=Rejected