# Market Data Feed Protocol Specification

- **Version:** 1.0
- **Transport:** UDP
- **Endianness:** Big-endian (network byte order)
- **Market Data Level:** Level 2 (aggregated price levels)

---

## 1. Overview

This document specifies the wire protocol for the **Market Data (MD) Feed**, used by clients to receive real-time order book updates and snapshots from the matching engine.

The feed is:

* **Unidirectional** (server → client)
* **Best-effort** (UDP)
* **Sequenced**
* **Instrument-scoped**
* **Level 2 only** (price + aggregated quantity)

The protocol supports:

* Incremental **delta updates**
* Periodic **full book snapshots** for recovery

Level 3 (order-level) data is explicitly out of scope for version 1.

---

## 2. Transport and Encoding

### 2.1 Transport

* Messages are sent over **UDP**
* Each UDP datagram contains exactly **one market data message**
* Messages must fit within a single UDP packet (no fragmentation)

### 2.2 Endianness

All multi-byte integer fields are encoded in **big-endian** (network byte order).

Single-byte fields (`uint8_t`) are endian-agnostic.

---

## 3. Message Framing

Each message consists of:

```
+---------------------+
| MarketDataHeader    | 16 bytes
+---------------------+
| Payload             | payloadLength bytes
+---------------------+
```

---

## 4. MarketDataHeader

### 4.1 Layout (16 bytes)

| Field          | Type   | Size | Description                              |
| -------------- | ------ | ---- | ---------------------------------------- |
| sequenceNumber | uint64 | 8    | Monotonically increasing sequence number |
| instrumentID   | uint32 | 4    | Instrument identifier                    |
| payloadLength  | uint16 | 2    | Length of payload in bytes               |
| mdMsgType      | uint8  | 1    | Message type                             |
| version        | uint8  | 1    | Protocol version                         |

Total size: **16 bytes**

---

### 4.2 Message Types

```cpp
enum class MDMsgType : uint8_t {
    DELTA    = 0,
    SNAPSHOT = 1
};
```

---

### 4.3 Sequencing Rules

* `sequenceNumber` is **global per feed**
* Increments by **exactly 1** per message
* Gaps indicate **packet loss**
* Clients may:

  * Ignore the gap and continue
  * Or wait for the next snapshot to resynchronize

---

## 5. Delta Messages

Delta messages represent **incremental changes** to the order book.

### 5.1 Delta Payload Layout (24 bytes)

```
+-------------------+
| priceLevel  (8)   |
| amountDelta (8)   |
| deltaType   (1)   |
| side        (1)   |
| padding     (6)   |
+-------------------+
```

| Field       | Type   | Description                |
| ----------- | ------ | -------------------------- |
| priceLevel  | uint64 | Price level being modified |
| amountDelta | uint64 | Quantity change            |
| deltaType   | uint8  | ADD or REDUCE              |
| side        | uint8  | BUY or SELL                |
| padding     | bytes  | Must be zero               |

```cpp
enum class MDDeltatype : uint8_t {
    ADD    = 0,
    REDUCE = 1
};
```

---

### 5.2 Semantics

* **ADD**

  * Increase quantity at `priceLevel` on `side`
  * If price level does not exist, it is created

* **REDUCE**

  * Decrease quantity at `priceLevel` on `side`
  * If resulting quantity becomes zero, the level is removed

Clients must assume deltas are applied **in sequence order**.

---

## 6. Snapshot Messages

Snapshot messages provide a **complete view of the order book** for an instrument.

They are intended for:

* Initial book build
* Recovery after packet loss
* Periodic consistency checks

---

### 6.1 Snapshot Payload Layout

```
+-------------------+
| SnapshotHeader    | 8 bytes
+-------------------+
| Bid Levels[]      | bidCount × 16 bytes
+-------------------+
| Ask Levels[]      | askCount × 16 bytes
+-------------------+
```

---

### 6.2 SnapshotHeader (8 bytes)

| Field    | Type   | Description                           |
| -------- | ------ | ------------------------------------- |
| bidCount | uint16 | Number of bid price levels            |
| askCount | uint16 | Number of ask price levels            |
| reserved | uint32 | Must be zero (alignment / future use) |

---

### 6.3 SnapshotLevel (16 bytes)

| Field | Type   | Description         |
| ----- | ------ | ------------------- |
| price | uint64 | Price level         |
| qty   | uint64 | Aggregated quantity |

---

### 6.4 Ordering Rules

* **Bids**

  * Sorted from **best to worst**
  * Highest price first

* **Asks**

  * Sorted from **best to worst**
  * Lowest price first

This ordering is guaranteed by the sender.

---

### 6.5 Snapshot Consistency

* A snapshot represents a **fully consistent book state**
* Snapshots are not interleaved with deltas
* After applying a snapshot, clients may resume applying deltas immediately

---

## 7. Client Recovery Model

Clients are expected to:

1. Track `sequenceNumber`
2. Detect gaps
3. On gap:

   * Continue applying deltas (best effort), or
   * Discard local book and wait for next snapshot

Snapshots are sufficient to fully reconstruct book state.

---

## 8. Versioning

* `version` field in `MarketDataHeader` identifies protocol version
* Version `0x01` corresponds to this document
* Backward-incompatible changes require a version bump

---

## 9. Guarantees and Non-Guarantees

### Guaranteed

* Message ordering (per feed)
* Internal consistency of each message
* Snapshot completeness

### Not Guaranteed

* Packet delivery
* Duplicate suppression
* Timely arrival

---

## 10. Explicit Non-Goals (v1)

* Level 3 (order-level) data
* Trade prints
* Market statistics
* Client acknowledgements
* Flow control

---

