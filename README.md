# MiniExchange (v3)

> [!NOTE]
> The previous version was archived due to several issues, this version (v3) is a rewrite that aims to address those issues with more robust implementation, more rigorous testing and cleaner architecture. Check out the branch `v2-archive` to see the documents.

## Updated Wire Specification [Read More](docs/wire-spec.md)

## Bug Fixes/Improvements

- Improved protocol handling logic
- InstrumentID included in all message types
- Removing orders from the `orderMap_` registry happens in the same place that the removal from the bids/asks happens
- Tested message layouts
- `MathResult` objects contain price information
- Relevant messages contain `ClientOrderID` fields for easier tracking and consistency between server-client
- Improved client-side architecture
- Client side memory bugs fixed (uninitialized fields)
- Market observer -- keeps a level 2 order book
- Level 2 market data (UDP multicast)
- Client capable of receiving and processing market data
- Cleaner shutdown
- Better order validation

## Planned features

- Market data front-end -> order book visualizations, market dynamics visualization (price, liquidity, etc)
- Client side python bindings for scriptable client strategies
- Scenario testing/running framework (making the system usable)
- Distributed server -> separating the observer and the market data components into a separate process with a message queue in a shared memory region between them and the matching engine
- (more testing)
