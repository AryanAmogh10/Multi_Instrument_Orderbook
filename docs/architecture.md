# Architecture

## Layers

1. **Gateway** — TCP server, binary protocol decode/encode, per-connection session state machine.
2. **Matching Engine Core** — `InstrumentRegistry` + one `OrderBook` per instrument + the `Engine` orchestrator that routes orders by `InstrumentId`. `ShardedMatcher` runs N engines across worker threads.

## Threading model

Sharded: N matching threads, each owns a disjoint subset of instruments via consistent hashing (`instrument_id % N`). Per-instrument matching is single-threaded, so there are no locks inside the match loop. Order ingestion into each shard uses a single-producer/single-consumer wait-free ring buffer.

## Key constraints

- Integer ticks for price — never floats.
- The wire protocol carries `protocol_version` from the first message.
- Lock-free structures are confined to cross-thread ingestion; matching itself stays serial per instrument.
- Orders are drawn from a pre-allocated `Pool`, so the hot path performs no heap allocation.
