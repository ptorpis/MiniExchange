# MiniExchange (v3)

> [!NOTE]
> The previous version was archived due to several issues, this version (v3) is a rewrite that aims to address those issues with more robust implementation, more rigorous testing and cleaner architecture. Check out the branch `v2-archive` to see the documents.

## Updated Wire Specification [Read More](docs/wire-spec.md)

## Bug Fixes/Improvements

- Improved protocol handling logic
- InstrumentID included in all message types
- Removing orders from the `orderMap_` registry happens in the same place that the 
- Tested message layouts
- `MathResult` objects contain price information
- Relevant messages contain `ClientOrderID` fields for easier tracking and consistency between server-client
- Improved client-side architecture
- Client side memory bugs fixed (uninitialized fields)
- Market observer, which keeps a level 2 order book, plans to use this for a market data feed implementation
- Cleaner shutdown
- Better order validation
