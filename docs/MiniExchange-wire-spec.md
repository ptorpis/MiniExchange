
# MiniExchange Wire Spec v1.0

**Transport:** TCP

**Framing:** fixed 8-byte header + `payload_len` bytes. Read header first, then exact payload.

**Endianness:** **big-endian** (network byte order).

**Identifiers:** Server Assigned 64 bit unsigned integer.

**Timestamps:** `int64_t` microseconds since Unix epoch (signed 8 bytes).

**Price/Qty:** integers (ticks and integer qty). Price = `int64_t` ticks. Qty = `int64_t` units (Validated that for all orders Price and Quantity is larger than 0).

**Server IDs:** server assigned IDs are `uint64_t` (8 bytes) used in internal references and notifications.

**Message types:** `uint8_t` in header. First byte in each message.

## HMAC Signing

All messages transmitted over the protocol are authenticated using HMAC (Hash-based Message Authentication Code) to ensure integrity and authenticity. The HMAC is computed using SHA-256 and a pre-shared secret key.

### 1. Scope of HMAC

The HMAC is computed over:

- The message header (all fields, in network byte order / big-endian)
- The message payload (all fields, in network byte order / big-endian)
- Important: The HMAC field itself is excluded from the computation.
- After computation, the resulting 32-byte HMAC is appended to the end of the message.

### 2. Byte Order

All numeric fields in both the header and payload must be represented in network byte order (big-endian) before computing the HMAC. This ensures consistency between different platforms.

### 3. HMAC Computation

The HMAC is computed as follows:


```
HMAC = HMAC_SHA256(key, header_bytes || payload_bytes)
```

Where:

- `key` is the pre-shared secret key.
- `header_bytes` is the serialized byte representation of the message header.
- `payload_bytes` is the serialized byte representation of the message payload.
- `||` represents byte concatenation.

The computed HMAC is 32 bytes long and is appended to the message after the payload.

Visually:

```
+----------------+----------------+----------------+
|   Header (BE)  |  Payload (BE)  |      HMAC      |
+----------------+----------------+----------------+
|   (N bytes)    |   (M bytes)    |   (32 bytes)   |
+----------------+----------------+----------------+
         ^                ^
         |                |
      Signed bytes: header + payload
```

Reference `ProtocolHandler::computeHMAC_()` in `protocol/protocolHandler.cpp`.

---

## Primitive sizes (wire)

- `uint8_t`: 1 byte
- `uint16_t`: 2 bytes (BE)
- `uint32_t`: 4 bytes (BE)
- `uint64_t`: 8 bytes (BE)
- `int64_t`: 8 bytes (BE)
- `Timestamp`: `uint64_t` us since epoch (BE)
- `uint8_t[n]`: Array of `n` bytes


## Header (16 bytes)

All messages start with this 16 byte header:

- Message Type: `uint8_t` (see [table](#message-types) for all the message codes)
- Protocol version flag: `uint8_t` [`0x01`=v1.0]
- Reserved flags: `uint8_t[2]`
- Payload Length (bytes): `uint16_t`
- Client Message Sequence Number: `uint32_t`
- Server Message Sequence Number: `uint32_t`
- Reserved/Padding: `uint8_t[2]`


Protocol version | Header flag 
--- | --- |
v1.0 | 1

## Message Types

| Message Type  | Code (dec/hex) | Direction | Payload Length | Payload Layout| Message Size |
| ------------- | -------------- | --------- | -------------- | ------------- | ------------ |
| **HELLO** | 1 / `0x01`     | client -> server | 48      | API Key: `uint8_t[16]` <br> HMAC: `uint8_t[32]`   | 64   |
| **HELLO_ACK**   | 2 / `0x02`     | server -> client| 48       | Server Client ID `uint64_t` <br> Status: `uint8_t` <br> Padding: `uint8_t[7]` <br> HMAC: `uint8_t[32]`    | 64           |
| **HEARTBEAT**     | 3 / `0x03`     | client -> server| 48      | Server Client ID `uint64_t` <br> Padding `uint8_t[8]` <br> HMAC: `uint8_t[32]`    | 64           |
| **LOGOUT**        | 4 / `0x04`     | client -> server| 48      | Server Client ID `uint64_t` <br> Padding `uint8_t[8]`<br> HMAC: `uint8_t[32]`     | 64           |
| **LOGOUT_ACK**      | 5 / `0x05`     | server -> client| 48    | Server Client ID `uint64_t` <br> Status `uint8_t` <br> Padding `uint8_t[7]` <br> HMAC: `uint8_t[32]` | 64           |
| **NEW_ORDER**    | 10 / `0x0A`    | client -> server | 80      | Server Client ID `uint64_t` <br> Instrument ID `uint32_t` <br> Order Side `uint8_t` <br> Order Tyoe `uint8_t` <br> Quantity `int64_t` <br> Price `int64_t` <br> Time-in-Force `uint8_t` <br> Good-till Date `uint64_t` <br> Padding `uint8_t[9]` <br> HMAC `uint8_t[32]`| 96    |
| **ORDER_ACK** | 11 / `0x0B`  | server -> client | 80  | Server Client ID `uint64_t` <br> Instrument ID `uint32_t` <br>Server Order ID `uint64_t` <br> Status `uint8_t` <br> Accepted Price `int64_t` <br> Accepted Quantity `int64_t` <br> Server Time `uint64_t` <br>  Padding `uint8_t[3]`<br> HMAC `uint8_t[32]`| 96   |
| **CANCEL_ORDER**    | 12 / `0x0C`    | client -> server | 64   | Server Client ID `uint64_t`<br> Server Order ID: `uint64_t` <br> Padding `uint8_t[16]` <br> HMAC: `uint8_t[32]` | 80      |
| **CANCEL_ACK**     | 13 / `0x0D`    | server -> client | 64    | Server Client ID `uint64_t` <br> Server Order ID: `uint64_t` <br> Status: `uint8_t` <br> Padding: `uint8_t[15]` <br> HMAC: `uint8_t[32]`  | 80   |
| **MODIFY_ORDER** | 14 / `0x0E`    | client -> server | 64      | Server Client ID `uint64_t` <br>Server Order ID: `uint64_t` <br> New Quantity: `int64_t` <br> New Price `uint64_t` <br> HMAC: `uint8_t[32]`    | 80           |
| **MODIFY_ACK**     | 15 / `0x0F`    | server -> client | 80       | Server Client ID `uint64_t` <br> Old Server Order ID: `uint64_t` <br> New Server Order ID: `uint64_t` <br> New Quantity: `int64_t` <br> New Price `int64_t` <br> Status: `uint8_t` <br> Padding: `uint8_t[7]` <br> HMAC: `uint8_t[32]`                             | 96           |
| **TRADE**     | 20 / `0x14`    | server - client | 80      | Server Client ID: `uint64_t` <br> Trade ID: `uint64_t` <br> Server Order ID: `uint64_t` <br> Filled Qty: `int64_t` <br> Filled Price: `int64_t` <br> Timestamp (us): `uint64_t` <br> HMAC: `uint8_t[32]` | 96           |

For further reference, consult: `protocol/messages.hpp` + `protocol/client/clientMessages.hpp` + `protocol/server/serverMessages.hpp`, where the structs are defined.

## Status Codes

Found under namespace `statusCodes`.

| Status Code | HelloAckStatus  | LogoutAckStatus | OrderAckStatus    | CancelAckStatus   | ModifyAckStatus   |
| ----------: | --------------- | --------------- | ----------------- | ----------------- | ----------------- |
|        0x00 | NULLSTATUS      | NULLSTATUS      | NULLSTATUS        | NULLSTATUS        | NULLSTATUS        |
|        0x01 | ACCEPTED        | ACCEPTED        | ACCEPTED          | ACCEPTED          | ACCEPTED          |
|        0x02 | INVALID_HMAC    | INVALID_HMAC    | INVALID           | INVALID           | INVALID           |
|        0x03 | INVALID_API_KEY | -               | OUT_OF_ORDER      | NOT_FOUND         | NOT_FOUND         |
|        0x04 | OUT_OF_ORDER    | OUT_OF_ORDER    | NOT_AUTHENTICATED | NOT_AUTHENTICATED | NOT_AUTHENTICATED |
|        0x05 | ILL_FORMED      | -               | -                 | OUT_OF_ORDER      | OUT_OF_ORDER      |
