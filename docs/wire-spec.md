# Messaging Protocol Specification

This document describes the rules that are applied when incoming messages are handled by the server. Generally clients send requests, like for placing a new order, the server reads this message, executes the desired operation on the market, then generates a response "ack" message and sends this back to the client that sent the original message.

Clients that commit protocol violations are either disconnected, or bad messages are ignored. This is subject to change as I develop more robust fault tolerance into the messaging system.

## Overview

The transport layer of the exchange's protocol is **TCP** to ensure reliability. Messages are **sequenced** both by the server and the client for extra continuity checking. Messages are of **fixed-per-type** lengths and follow a **header-payload** structure. Byte ordering over the wire is **Big-endian** (BE).

## Message Header

Message headers are always **16 bytes** for all message types. Here is the exact layout:

Field|Size|Description
-|-|-
Message Type|1 byte (`std::uint8_t`) |Single byte corresponds to the type of the message
Protocol Version Flag | 1 byte | Single byte flag to indicate the version of the protocol
Payload Length | 2 bytes (`std::uint16_t`) | Message payload length in bytes
Client Sequence Number | 4 bytes (`std::uint32_t`) | Unique (per session) for every message the client sends
Server Sequence Number | 4 bytes | Unique (per session) for every message the server sends
Padding/Reserved|4 bytes|Reserved space for potential extensions

## Message Types

All messages are 8-byte aligned with a fixed layout enforced by compiler directives.

Message Type|Code (dec/hex)|Payload Length|Payload Layout (Field Length)|Message Size
-|-|-|-|-
HELLO|1/0x1|8|Padding/Reserved (8) |24
HELLO_ACK|2/0x2|16|Server Client ID (8) <br> Status (1) <br> Padding (7)|32
LOGOUT|3/0x3|8|Server Client ID (8) | 24
LOGOUT_ACK|4/0x4|16|Server Client ID (8) <br> Status (1) <br> Padding (7) | 32
NEW_ORDER|10/0xA|48|Server Client ID (8) <br> Client OrderID (8) <br> Instrument ID (4) <br> Order Side (1) <br> Order Type (1) <br> Time In Force (1) <br> Padding (1) <br> Quantity (8) <br> Price (8) <br> Good Till Date (8) | 64
ORDER_ACK|11/0xB | 56 | Server Client ID (8) <br>  Server Order ID (8) <br> Client Order ID (8) <br> Accepted Price (8) <br> Remaining Quantity (8) <br> Server Time (8) <br> Instrument ID (4) <br> Status (1) <br> Padding (3) | 72
CANCEL_ORDER|12/0xC|32|Server Client ID (8) <br> Server Order ID (8) <br> Client Order ID (8) <br> Instrument ID (4) <br> Padding (4) | 48
CANCEL_ACK|13/0xD | 32| Server Client ID (8) <br> Server OrderID (8) <br> Client Order ID (8) <br> Instrument ID (4) <br> Status (1) <br> Padding (3) | 48
MODIFY_ORDER|14/0xE |
MODIFY_ACK|15/0xF
TRADE|16/0x10



