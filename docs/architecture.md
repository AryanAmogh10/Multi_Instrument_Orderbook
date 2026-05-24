# Architecture

> Populated as components land. Current state: Phase 0 (scaffold only).

## Layers

1. **Gateway** — TCP, auth, protocol decode/encode, session state. (Phase 3)
2. **Matching Engine Core** — InstrumentRegistry + per-instrument OrderBook + MatchingEngine orchestrator. (Phases 1–2)
3. **Trade Reporter / Market Data Publisher** — L1/L2/L3 + trade tape. (Phase 6)
4. **Persistence** — append-only order journal, trade ledger, replay. (Phase 7)

## Threading model

Sharded: N matching threads, each owns M instruments via consistent hashing. Per-instrument matching is single-threaded. Ingestion uses SPSC lock-free queues; trade publication uses MPSC.

## Key constraints

- Integer ticks for price — never floats.
- Wire protocol carries `protocol_version` from message #1.
- Lock-free is confined to ingestion + publication; matching itself stays serial per instrument.
