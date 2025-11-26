
# MiniExchange Wire Spec v1.0

## Primitive sizes (wire)

- `uint8_t`: 1 byte
- `uint16_t`: 2 bytes (BE)
- `uint32_t`: 4 bytes (BE)
- `uint64_t`: 8 bytes (BE)
- `int64_t`: 8 bytes (BE)
- `Timestamp`: `uint64_t` us since epoch (BE)
- `uint8_t[n]`: Array of `n` bytes


## Header

All messages start with this 16 byte header:

- Message Type: `uint8_t` (see [table](#message-types) for all the message codes)
- Protocol version flag: `uint8_t` [`0x01`=v1.0]
- Payload Length (bytes): `uint16_t`
- Client Message Sequence Number: `uint32_t`
- Server Message Sequence Number: `uint32_t`
- Reserved/Padding: `uint8_t[4]`


Protocol version | Header flag 
--- | --- |
v1.0 | 1

## Message Types

| Message Type  | Code (dec/hex) | Direction | Payload Length | Payload Layout| Message Size |
| ------------- | -------------- | --------- | -------------- | ------------- | ------------ |
| **HELLO** | 1 / `0x01`| client -> server | 16 | API Key: `uint8_t[16]` | 32 |
| **HELLO_ACK** | 2 / `0x02`| server -> client| 16 | Server Client ID `uint64_t` <br> Status: `uint8_t` <br> Padding: `uint8_t[7]` | 32 |
| **HEARTBEAT**     | 3 / `0x03`| client -> server| 8 | Server Client ID `uint64_t` | 24 |
| **LOGOUT**        | 4 / `0x04` | client -> server| 8 | Server Client ID `uint64_t` | 24 |
| **LOGOUT_ACK**      | 5 / `0x05`| server -> client| 16 | Server Client ID `uint64_t` <br> Status `uint8_t` <br> Padding `uint8_t[7]`| 32 |
| **NEW_ORDER**    | 10 / `0x0A`    | client -> server | 40      | Server Client ID `uint64_t` <br> Instrument ID `uint32_t` <br> Order Side `uint8_t` <br> Order Type `uint8_t` <br> Time-in-Force `uint8_t` <br> Padding `uint8_t[1]` <br> Quantity `int64_t` <br> Price `int64_t` <br> Good-till Date `uint64_t` | 56 |
| **ORDER_ACK** | 11 / `0x0B`  | server -> client | 48  | Server Client ID `uint64_t` <br> Server Order ID `uint64_t` <br> Accepted Price `int64` <br> Accepted Quantity `int64_t` <br> Server Time `uint64_t` <br> Instrument ID `uint32_t` <br> Status `uint8_t` <br>  Padding `uint8_t[3]`| 64 |
| **CANCEL_ORDER**  | 12 / `0x0C`| client -> server | 16 | Server Client ID `uint64_t`<br> Server Order ID: `uint64_t` | 32 |
| **CANCEL_ACK**    | 13 / `0x0D`    | server -> client | 24    | Server Client ID `uint64_t` <br> Server Order ID: `uint64_t` <br> Status: `uint8_t` <br> Padding: `uint8_t[7]` | 40   |
| **MODIFY_ORDER**  | 14 / `0x0E`    | client -> server | 32      | Server Client ID `uint64_t` <br>Server Order ID: `uint64_t` <br> New Quantity: `int64_t` <br> New Price `uint64_t` | 48|
| **MODIFY_ACK**    | 15 / `0x0F`    | server -> client | 48       | Server Client ID `uint64_t` <br> Old Server Order ID: `uint64_t` <br> New Server Order ID: `uint64_t` <br> New Quantity: `int64_t` <br> New Price `int64_t` <br> Status: `uint8_t` <br> Padding: `uint8_t[7]` | 56  |
| **TRADE**     | 20 / `0x14`    | server - client | 48      | Server Client ID: `uint64_t` <br> Trade ID: `uint64_t` <br> Server Order ID: `uint64_t` <br> Filled Qty: `int64_t` <br> Filled Price: `int64_t` <br> Timestamp (us): `uint64_t` | 56 |

For further reference, consult: `protocol/messages.hpp` + `protocol/client/clientMessages.hpp` + `protocol/server/serverMessages.hpp`, where the structs are defined.

## Status Codes

Found under namespace `statusCodes`.

| Status Code | HelloAckStatus  | LogoutAckStatus | OrderAckStatus    | CancelAckStatus   | ModifyAckStatus   |
| ----------: | --------------- | --------------- | ----------------- | ----------------- | ----------------- |
|        0x00 | NULLSTATUS      | NULLSTATUS      | NULLSTATUS        | NULLSTATUS        | NULLSTATUS        |
|        0x01 | ACCEPTED        | ACCEPTED        | ACCEPTED          | ACCEPTED          | ACCEPTED          |
|        0x02 | -    | -   | INVALID           | INVALID           | INVALID           |
|        0x03 | INVALID_API_KEY | -               | OUT_OF_ORDER      | NOT_FOUND         | NOT_FOUND         |
|        0x04 | OUT_OF_ORDER    | OUT_OF_ORDER    | NOT_AUTHENTICATED | NOT_AUTHENTICATED | NOT_AUTHENTICATED |
|        0x05 | ILL_FORMED      | -               | -                 | OUT_OF_ORDER      | OUT_OF_ORDER      |


## Rules of the Protocol

All messages start with the same 16 byte header. The layout of the message header can be found in [header](#header). The most important fields are the type, which is always the first byte, and the 2 sequences numbers, for the client and the server, respectively. When a message is sent, the sequence number should be incremented by one. When the message is being sent by the client, only the client sequence number should be incremented, similarly, when the server is sending the message, only the sever sequence number should be incremented.

The first sequence is always 1. This is because the `SessionManager` is initialized with the counter set to 0 and at each client session creation, this counter is incremented, and there is no distinction is made for the first client, so the IDs will start from 1.

Both sever and client sequence numbers are 32 bit unsigned integers, when the sequence number hits the limit, that it can hold, it should wrap around to 0. The sequence number validation will use modulo wrapping to determine if the sequence number included in the message is correct.

Correct sequencing means that the number is incremented by exactly 1. If it is the same as the last one or incremented by more than one, the message is considered invalid, and will be acked with the status `OUT_OF_ORDER`.

### HELLO -- HELLO_ACK

`HELLO` messages are used as a login message by the protocol and is sent by the client. It includes the API key (as of version v1.0, the API key validation is stubbed out, and as convention, the field is usually `0x22`*16). When the server receives this message, it creates and initializes a new session for that client.

`HELLO_ACK` this message is sent by the server as a response for the `HELLO`.

The ack message contains the client ID that got assigned for the client, which is an 8 byte unsigned integer. This is the ID that the client should include in their messages for future requests.

The client ID is assigned based on a sequential counter found in the `SessionManager`. Note that this is not a secure way to assign IDs, and should only be used in local testing.

Only one `HELLO` message is accepted and responded to by the server, any subsequent `HELLO` messages from clients who are already authenticated are dropped (no response is generated)

### LOGOUT -- LOGOUT_ACK

The `LOGOUT` is sent by the client when they want to terminate their session with the exchange. 
