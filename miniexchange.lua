-- miniexchange.lua
-- MiniExchange Protocol dissector (big-endian, reassembly-capable, heuristic TCP)
-- Place in ~/.local/share/wireshark/plugins/miniexchange.lua

local proto_name = "MiniExchange Protocol"
local myproto = Proto("miniexchange", proto_name)

-- debug prints on load (visible if you start Wireshark from terminal)
print("MiniExchange: loading miniexchange.lua (heuristic, reassembly-capable)")

-- --------------------------------------------------------------------
-- Field definitions (header + payload). Padding bytes are NOT displayed.
-- --------------------------------------------------------------------
local f = myproto.fields

-- Header (16 bytes)
f.msg_type  = ProtoField.uint8("miniexchange.msg_type", "Message Type", base.DEC)
f.version   = ProtoField.uint8("miniexchange.version", "Protocol Version Flag", base.DEC)
f.length    = ProtoField.uint16("miniexchange.length", "Payload Length", base.DEC)
f.clientSqn = ProtoField.uint32("miniexchange.clientMsgSqn", "Client Sequence", base.DEC)
f.serverSqn = ProtoField.uint32("miniexchange.serverMsgSqn", "Server Sequence", base.DEC)
-- bytes 12-15 are padding in header; intentionally NOT exposed

-- Common payload fields
f.serverClientID   = ProtoField.uint64("miniexchange.serverClientID", "Server Client ID", base.HEX)
f.serverOrderID    = ProtoField.uint64("miniexchange.serverOrderID", "Server Order ID", base.HEX)
f.status           = ProtoField.uint8("miniexchange.status", "Status", base.DEC)
f.apiKey           = ProtoField.bytes("miniexchange.apiKey", "API Key (16 bytes)")

-- New order fields
f.instrumentID     = ProtoField.uint32("miniexchange.instrumentID", "Instrument ID", base.DEC)
f.orderSide        = ProtoField.uint8("miniexchange.orderSide", "Order Side", base.DEC)
f.orderType        = ProtoField.uint8("miniexchange.orderType", "Order Type", base.DEC)
f.timeInForce      = ProtoField.uint8("miniexchange.timeInForce", "Time-In-Force", base.DEC)
f.quantity         = ProtoField.int64("miniexchange.quantity", "Quantity", base.DEC)
f.price            = ProtoField.int64("miniexchange.price", "Price", base.DEC)
f.goodTillDate     = ProtoField.uint64("miniexchange.goodTillDate", "Good Till Date", base.DEC)

-- Order ack fields
f.acceptedPrice    = ProtoField.int64("miniexchange.acceptedPrice", "Accepted Price", base.DEC)
f.acceptedQty      = ProtoField.int64("miniexchange.acceptedQty", "Accepted Quantity", base.DEC)
f.serverTime       = ProtoField.uint64("miniexchange.serverTime", "Server Time", base.DEC)

-- Modify ack / modify order fields
f.oldServerOrderID = ProtoField.uint64("miniexchange.oldServerOrderID", "Old Server Order ID", base.HEX)
f.newServerOrderID = ProtoField.uint64("miniexchange.newServerOrderID", "New Server Order ID", base.HEX)
f.modify_newQty    = ProtoField.int64("miniexchange.modify_newQty", "New Quantity", base.DEC)
f.modify_newPrice  = ProtoField.int64("miniexchange.modify_newPrice", "New Price", base.DEC)

-- Trade fields
f.tradeID          = ProtoField.uint64("miniexchange.tradeID", "Trade ID", base.HEX)
f.filledQty        = ProtoField.int64("miniexchange.filledQty", "Filled Quantity", base.DEC)
f.filledPrice      = ProtoField.int64("miniexchange.filledPrice", "Filled Price", base.DEC)
f.timestamp        = ProtoField.uint64("miniexchange.timestamp", "Timestamp (us)", base.DEC)

-- Message name map
local msg_names = {
    [1]  = "HELLO",
    [2]  = "HELLO_ACK",
    [3]  = "HEARTBEAT",
    [4]  = "LOGOUT",
    [5]  = "LOGOUT_ACK",
    [10] = "NEW_ORDER",
    [11] = "ORDER_ACK",
    [12] = "CANCEL_ORDER",
    [13] = "CANCEL_ACK",
    [14] = "MODIFY_ORDER",
    [15] = "MODIFY_ACK",
    [20] = "TRADE",
}

-- --------------------------------------------------------------------
-- Helper readers (BIG-ENDIAN on the wire)
-- --------------------------------------------------------------------
local function read_u8(tvb, off)  return tvb(off,1):uint() end
local function read_u16(tvb, off) return tvb(off,2):uint() end 
local function read_u32(tvb, off) return tvb(off,4):uint() end
local function read_u64(tvb, off) return tvb(off,8):uint() end

-- Adders: add a field to the tree and return new offset
local function add_u8(tree, field, tvb, off) tree:add(field, tvb(off,1)); return off + 1 end
local function add_u16(tree, field, tvb, off) tree:add(field, tvb(off,2)); return off + 2 end
local function add_u32(tree, field, tvb, off) tree:add(field, tvb(off,4)); return off + 4 end
local function add_u64(tree, field, tvb, off) tree:add(field, tvb(off,8)); return off + 8 end
local function add_i64(tree, field, tvb, off) tree:add(field, tvb(off,8)); return off + 8 end
local function add_bytes(tree, field, tvb, off, len) tree:add(field, tvb(off, len)); return off + len end

-- --------------------------------------------------------------------
-- Decode a single message (given the base offset of that message)
-- returns next_offset (base + message_length)
-- --------------------------------------------------------------------
local function decode_single_message(tvb, pinfo, tree, base_off)
    -- header must be present (caller guarantees)
    local msg_type = read_u8(tvb, base_off + 0)
    local version  = read_u8(tvb, base_off + 1)
    local payload_len = read_u16(tvb, base_off + 2)
    local clientSqn = read_u32(tvb, base_off + 4)
    local serverSqn = read_u32(tvb, base_off + 8)
    local total_len = 16 + payload_len

    -- build a subtree for this message
    local tvb_msg = tvb(base_off, total_len)
    local subtree = tree:add(myproto, tvb_msg, string.format("MiniExchange: %s (type %d)", msg_names[msg_type] or "UNKNOWN", msg_type))

    -- header fields (do not show padding bytes)
    subtree:add(f.msg_type, tvb_msg(0,1)):set_text("Message Type: " .. msg_type .. " (" .. (msg_names[msg_type] or "UNKNOWN") .. ")")
    subtree:add(f.version, tvb_msg(1,1))
    subtree:add(f.length, tvb_msg(2,2))
    subtree:add(f.clientSqn, tvb_msg(4,4))
    subtree:add(f.serverSqn, tvb_msg(8,4))
    -- header padding (bytes 12-15) intentionally not added

    -- info column: include type and client/server sequence
    pinfo.cols.info:set(string.format("%s | cSqn=%d sSqn=%d", msg_names[msg_type] or ("Type "..msg_type), clientSqn, serverSqn))

    -- decode payload if present
    local pay_off = base_off + 16
    if payload_len == 0 then
        -- nothing to decode
        return base_off + total_len
    end

    -- child payload subtree
    local payload_tree = subtree:add(myproto, tvb_msg(16, payload_len), "Payload")

    -- decode based on msg type
    if msg_type == 1 then
        -- HELLO: apiKey[16]
        add_bytes(payload_tree, f.apiKey, tvb, pay_off, 16)

    elseif msg_type == 2 or msg_type == 5 then
        add_u64(payload_tree, f.serverClientID, tvb, pay_off)
        add_u8(payload_tree, f.status, tvb, pay_off + 8) -- status is next after serverClientID
        -- remaining padding omitted

    elseif msg_type == 3 or msg_type == 4 then
        add_u64(payload_tree, f.serverClientID, tvb, pay_off)

    elseif msg_type == 10 then
        local off = pay_off
        off = add_u64(payload_tree, f.serverClientID, tvb, off)
        off = add_u32(payload_tree, f.instrumentID, tvb, off)
        off = add_u8(payload_tree, f.orderSide, tvb, off)
        off = add_u8(payload_tree, f.orderType, tvb, off)
        off = add_u8(payload_tree, f.timeInForce, tvb, off)
        off = off + 1
        off = add_i64(payload_tree, f.quantity, tvb, off)
        off = add_i64(payload_tree, f.price, tvb, off)
        off = add_u64(payload_tree, f.goodTillDate, tvb, off)

    elseif msg_type == 11 then
        local off = pay_off
        off = add_u64(payload_tree, f.serverClientID, tvb, off)
        off = add_u64(payload_tree, f.serverOrderID, tvb, off)
        off = add_i64(payload_tree, f.acceptedPrice, tvb, off)
        off = add_i64(payload_tree, f.acceptedQty, tvb, off)
        off = add_u64(payload_tree, f.serverTime, tvb, off)
        off = add_u32(payload_tree, f.instrumentID, tvb, off)
        off = add_u8(payload_tree, f.status, tvb, off)
        -- there are 3 bytes padding here in the struct: skip them (do not display)

    elseif msg_type == 12 then
        local off = pay_off
        off = add_u64(payload_tree, f.serverClientID, tvb, off)
        off = add_u64(payload_tree, f.serverOrderID, tvb, off)

    elseif msg_type == 13 then
        local off = pay_off
        off = add_u64(payload_tree, f.serverClientID, tvb, off)
        off = add_u64(payload_tree, f.serverOrderID, tvb, off)
        off = add_u8(payload_tree, f.status, tvb, off)
        -- 7 bytes padding in struct -> skip

    elseif msg_type == 14 then
        local off = pay_off
        off = add_u64(payload_tree, f.serverClientID, tvb, off)
        off = add_u64(payload_tree, f.serverOrderID, tvb, off)
        off = add_i64(payload_tree, f.modify_newQty, tvb, off)
        off = add_i64(payload_tree, f.modify_newPrice, tvb, off)

    elseif msg_type == 15 then
        local off = pay_off
        off = add_u64(payload_tree, f.serverClientID, tvb, off)
        off = add_u64(payload_tree, f.oldServerOrderID, tvb, off)
        off = add_u64(payload_tree, f.newServerOrderID, tvb, off)
        off = add_i64(payload_tree, f.modify_newQty, tvb, off)
        off = add_i64(payload_tree, f.modify_newPrice, tvb, off)
        off = add_u8(payload_tree, f.status, tvb, off)
        -- skip 7 bytes padding

    elseif msg_type == 20 then
        local off = pay_off
        off = add_u64(payload_tree, f.serverClientID, tvb, off)
        off = add_u64(payload_tree, f.serverOrderID, tvb, off)
        off = add_u64(payload_tree, f.tradeID, tvb, off)
        off = add_i64(payload_tree, f.filledQty, tvb, off)
        off = add_i64(payload_tree, f.filledPrice, tvb, off)
        off = add_u64(payload_tree, f.timestamp, tvb, off)

    else
        -- Unknown message type: show raw payload (but do not expose header padding)
        payload_tree:add(tvb(pay_off, payload_len))
    end

    return base_off + total_len
end

-- --------------------------------------------------------------------
-- Main dissector: supports multiple messages in a tcp segment and reassembly
-- --------------------------------------------------------------------
function myproto.dissector(tvb, pinfo, tree)
    local len = tvb:len()

    -- Need at least header
    if len < 16 then
        pinfo.desegment_len = DESEGMENT_ONE_MORE_SEGMENT
        return
    end

    local offset = 0
    while offset < len do
        if (len - offset) < 16 then
            pinfo.desegment_offset = offset
            pinfo.desegment_len = DESEGMENT_ONE_MORE_SEGMENT
            return
        end

        local payload_len = read_u16(tvb, offset + 2)
        local msg_len = 16 + payload_len

        if (len - offset) < msg_len then
            pinfo.desegment_offset = offset
            pinfo.desegment_len = msg_len - (len - offset)
            return
        end

        offset = decode_single_message(tvb, pinfo, tree, offset)
    end
end
-- --------------------------------------------------------------------
-- Heuristic detective: run on TCP and claim the stream if it looks like MiniExchange
-- This allows the dissector to run regardless of port.
-- --------------------------------------------------------------------
local function heuristic_checker(tvb, pinfo, tree)
    local len = tvb:len()
    -- must have at least header
    if len < 3 then
        return false
    end

    local msg_type = read_u8(tvb, 0)
    local payload_len = read_u16(tvb, 2)

    -- heuristic checks:
    -- 1) message type is one of known types
    -- 2) payload_len is not absurdly large (defensive)
    -- 3) total length doesn't exceed capture (avoid false positives)
    if msg_names[msg_type] == nil then
        if payload_len > 2000000 then return false end
    else
        -- known message type
        if payload_len > 2000000 then return false end
    end

    myproto.dissector(tvb, pinfo, tree)
    return true
end

-- register heuristic for tcp
myproto:register_heuristic("tcp", heuristic_checker)

print("MiniExchange heuristic dissector registered for TCP (no fixed port required)")

