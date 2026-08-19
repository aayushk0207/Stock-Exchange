# Stock Exchange Matching Engine (C++20)

A high-performance, low-latency, multi-threaded C++20 stock exchange matching engine. It features asynchronous network ingestion, pre-trade risk checks, concurrent matching across multiple symbol order books, asynchronous write-ahead logging (WAL), state recovery, and an asynchronous execution dispatch pipeline.

---

## Architectural Design

The matching engine is built on an event-driven, pipelined architecture using a thread pool and lock-free queues to pass messages between processing boundaries. Critical sections are isolated, and lock contention is localized to the order books on a per-symbol basis.

### End-to-End System Pipeline

```mermaid
flowchart TD
    subgraph Client Gateway
        C[TCP Clients] <-->|Binary Packets| S[Gateway Server / Sessions]
    end

    subgraph Risk Verification
        S -->|1. Validate| RC[Risk Checker]
        RC -->|Rejected| S
    end

    subgraph Dispatch
        RC -->|2. Passed| TQ[ThreadPool Bounded Queue]
        TQ -->|3. Dequeue| TP[Thread Pool Workers]
    end

    subgraph Matching & Persistence
        TP -->|4. Log Request| WAL[WAL Write-Ahead Log]
        TP -->|5. Match / Execute| OB[Order Books]
        OB -.->|Price-Time Priority| OB
        OB -->|6. Log Match| WAL
        OB -->|7. Generate Reports| EP[Execution Pipeline]
    end

    subgraph Downstream Dispatch
        EP -->|8. Async Dispatch| S
    end

    classDef default fill:#1E1E24,stroke:#3C3C43,stroke-width:1px,color:#E1E1E6;
    classDef client fill:#2A3F3D,stroke:#4C8C80,color:#A3E2D5;
    classDef risk fill:#4A2B2D,stroke:#C26D73,color:#FFB6B9;
    classDef worker fill:#3E3725,stroke:#9E874B,color:#FFE29A;
    classDef log fill:#2E263D,stroke:#7D5FAD,color:#E1CCFF;

    class C,S client;
    class RC risk;
    class TQ,TP worker;
    class WAL,EP log;
```

---

## Core Components

### 1. Client Gateway & Serialization
- **`GatewayServer`**: An asynchronous TCP server using header-only `asio` that listens for client connections.
- **`Session`**: Manages individual TCP socket connections, parses incoming byte streams into domain messages, and handles asynchronous writes to clients.
- **`BinarySerializer`**: Implements custom binary serialization for speed and network efficiency. Message structures (`NetOrder`, `NetCancel`, `NetModify`, `NetExecutionReport`) are packed using `#pragma pack(push, 1)` to eliminate compiler padding.
- **`ConnectionManager`**: A thread-safe, centralized registry tracking all active client sessions by ID.

### 2. Pre-Trade Risk Management
- **`RiskChecker`**: A singleton validation boundary. To prevent fat-finger trades or unauthorized activity, it inspects every incoming order prior to submission:
  - Allowed trading symbol verification.
  - Price boundaries checking (`MIN_PRICE` to `MAX_PRICE`).
  - Quantity bounds checking (1 to `MAX_QUANTITY`).
  - Duplicate Order ID identification.
  - Maximum Order Value limit verification (e.g., $5,000,000 maximum order value).
- If validation fails, an `ExecutionReport` with `OrderStatus::Rejected` is immediately dispatched back to the client, avoiding matching engine queue overhead.

### 3. Threading & Concurrency Model
- **`ThreadPool`**: Utilizes a configurable number of worker threads that dequeue and process client tasks.
- **`SafeQueue`**: A bounded blocking queue (`constants::MAX_QUEUE_CAPACITY`) protecting workers from resource starvation or Out-Of-Memory (OOM) failures under heavy traffic bursts.
- **`BookRegistry`**: Manages the instances of active order books in a thread-safe registry, creating them on-demand when symbols are requested.

### 4. Matching Engine Core
- **`OrderBook`**: Uses standard double-sided maps for Bids (sorted descending) and Asks (sorted ascending). It features a dedicated `std::mutex` per order book. Lock contention is confined to the active symbol, allowing different threads to match orders for `AAPL`, `MSFT`, and `GOOG` simultaneously without blocking each other.
- **`OrderIndex`**: A map indexing order IDs to their list iterators inside the book. Enables $O(1)$ lookups for cancellation and modify requests.
- **`MatchingEngine`**: A static utility applying Price-Time Priority (FIFO) matching. Execution price is dictated by the resting order (which has the older timestamp).

### 5. Write-Ahead Logging (WAL) & Replay Recovery
- **`WAL`**: Appends state-changing transactions and execution fills into a binary log file. Ingestion is offloaded to a lock-free Multi-Producer Single-Consumer (`MPSCQueue`) so that worker threads do not stall on disk I/O.
- **`ReplayEngine`**: Reads binary WAL log files sequentially to reconstruct identical `OrderBook` states on startup. It resets the `TradeIDGenerator` to keep recovered trade IDs matching original execution numbers.

```mermaid
flowchart LR
    WAL[(WAL Log File)] -->|1. Read binary entries| RE[Replay Engine]
    RE -->|2. Reset ID generator| TG[TradeIDGenerator]
    RE -->|3. Replay Order Events| BR[Book Registry]
    BR -->|4. Rebuild state| OB[Order Books]

    classDef default fill:#1E1E24,stroke:#3C3C43,stroke-width:1px,color:#E1E1E6;
    classDef recovery fill:#2D3E50,stroke:#5dade2,color:#d5dbdb;
    class WAL,RE,TG,BR,OB recovery;
```

### 6. Execution Dispatching
- **`ExecutionPipeline`**: A high-performance dispatch pipeline. Workers publish execution reports and trades into separate lock-free MPSC queues. A dedicated background thread processes them asynchronously to write data to clients through `ConnectionManager`.

---

## Build & Execution Instructions

### Requirements
- A compiler supporting **C++20** (GCC 10+, Clang 10+, or MSVC 2019+).
- Threads and Winsock library support.

### Building & Running the Project
Run the single-command build script from the root directory:

```powershell
.\build.bat
```

This compiles all source files with C++20 and automatically executes `exchange_demo.exe` in your terminal.

