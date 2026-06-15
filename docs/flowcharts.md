# OS-Chat-Application - Flowcharts & Block Diagrams

> Operating System Lab | Iqra University | Department of Software Engineering

---

## 1. System Block Diagram

```mermaid
graph TB
    subgraph "Network Layer"
        NET["TCP/IP Socket<br/>(Port 9090)"]
    end

    subgraph "Server Application"
        ACCEPT["Accept Loop<br/>(Main Thread)"]
        AUTH["Authentication<br/>Module"]
        TH1["Client Thread 1"]
        TH2["Client Thread 2"]
        THN["Client Thread N"]
        RM["Room Manager"]
        MQ["Message Queue<br/>(Circular Buffer)"]
        CM["Client Manager<br/>(Dynamic Array)"]
        DB["User Database<br/>(users.dat)"]
    end

    subgraph "Clients"
        C1["Client 1<br/>(Send/Recv Threads)"]
        C2["Client 2<br/>(Send/Recv Threads)"]
        CN["Client N<br/>(Send/Recv Threads)"]
    end

    C1 <-->|TCP| NET
    C2 <-->|TCP| NET
    CN <-->|TCP| NET
    NET --> ACCEPT
    ACCEPT -->|spawn| TH1
    ACCEPT -->|spawn| TH2
    ACCEPT -->|spawn| THN
    TH1 --> AUTH
    TH2 --> AUTH
    THN --> AUTH
    AUTH <--> DB
    TH1 <--> CM
    TH2 <--> CM
    THN <--> CM
    CM <--> RM
    CM <--> MQ

    style NET fill:#4a9eff,stroke:#333,color:#fff
    style ACCEPT fill:#ff6b6b,stroke:#333,color:#fff
    style AUTH fill:#ffd93d,stroke:#333,color:#000
    style MQ fill:#6bcb77,stroke:#333,color:#fff
    style RM fill:#6bcb77,stroke:#333,color:#fff
    style CM fill:#6bcb77,stroke:#333,color:#fff
    style DB fill:#c084fc,stroke:#333,color:#fff
```

## 2. Client-Server Communication Sequence

```mermaid
sequenceDiagram
    participant C as Client
    participant S as Server
    participant R as Room Manager
    participant A as Auth Module
    participant O as Other Clients

    Note over C,S: Connection Phase
    C->>S: TCP Connect (SYN)
    S->>C: Accept (SYN-ACK)
    C->>S: ACK

    Note over C,A: Authentication Phase
    C->>S: MSG_AUTH_REQUEST (login/register)
    S->>A: Verify credentials
    A->>S: Auth result
    S->>C: MSG_AUTH_RESPONSE (success/fail)

    Note over C,R: Room Join Phase
    S->>R: Add user to "general"
    S->>O: MSG_SYSTEM ("user joined")
    S->>C: MSG_SYSTEM ("Welcome!")

    Note over C,O: Messaging Phase
    C->>S: MSG_BROADCAST ("Hello!")
    S->>R: Resolve room members
    S->>O: MSG_BROADCAST (forwarded)

    C->>S: MSG_PRIVATE (to specific user)
    S->>O: MSG_PRIVATE (delivered)

    Note over C,S: Room Switch
    C->>S: MSG_JOIN_ROOM ("project-os")
    S->>R: Move user to new room
    S->>O: MSG_SYSTEM ("user left/joined")
    S->>C: MSG_SYSTEM ("Joined room")

    Note over C,S: Disconnect Phase
    C->>S: MSG_DISCONNECT
    S->>R: Remove from room
    S->>O: MSG_SYSTEM ("user left")
    S->>S: free(client) / close(fd)
```

## 3. Server Main Loop Flowchart

```mermaid
flowchart TD
    START([Server Start]) --> INIT["Initialize:<br/>- Create socket<br/>- Bind to port<br/>- Listen<br/>- Create default room<br/>- Init message queue"]
    INIT --> ACCEPT{"accept() <br/>new connection?"}

    ACCEPT -->|Yes| CHECK{"client_count <br/>< MAX_CLIENTS?"}
    CHECK -->|Yes| ALLOC["calloc(1, sizeof client_t)"]
    ALLOC --> SPAWN["pthread_create()<br/>→ client_handler()"]
    SPAWN --> ACCEPT

    CHECK -->|No| REJECT["Send 'Server Full'<br/>close(fd)"]
    REJECT --> ACCEPT

    ACCEPT -->|Error| ERCHK{"server_running?"}
    ERCHK -->|Yes| ACCEPT
    ERCHK -->|No| SHUTDOWN

    SHUTDOWN["Shutdown:<br/>- Close all clients<br/>- Destroy queues<br/>- Close server socket"]
    SHUTDOWN --> DONE([Exit])

    style START fill:#6bcb77,color:#fff
    style DONE fill:#ff6b6b,color:#fff
    style ALLOC fill:#ffd93d,color:#000
    style SPAWN fill:#4a9eff,color:#fff
```

## 4. Client Handler Thread Flowchart

```mermaid
flowchart TD
    START([Thread Start]) --> AUTH["handle_auth()"]
    AUTH --> AUTHOK{"Authenticated?"}
    AUTHOK -->|No| CLEANUP1["close(fd), free(client)"]
    CLEANUP1 --> END1([Thread Exit])

    AUTHOK -->|Yes| JOIN["Join default room<br/>Broadcast 'user joined'"]
    JOIN --> RECV["recv_message()"]
    RECV --> RECVOK{"bytes > 0?"}

    RECVOK -->|No| CLEANUP2

    RECVOK -->|Yes| SWITCH{"Message Type?"}

    SWITCH -->|MSG_BROADCAST| BC["broadcast_to_room()<br/>(exclude sender)"]
    SWITCH -->|MSG_PRIVATE| PM["find_client_by_name()<br/>send to target"]
    SWITCH -->|MSG_JOIN_ROOM| JR["handle_join_room()"]
    SWITCH -->|MSG_CREATE_ROOM| CR["create_room()"]
    SWITCH -->|MSG_LIST_USERS| LU["handle_list_users()"]
    SWITCH -->|MSG_LIST_ROOMS| LR["handle_list_rooms()"]
    SWITCH -->|MSG_DISCONNECT| CLEANUP2
    SWITCH -->|MSG_HEARTBEAT| HB["Send pong"]

    BC --> RECV
    PM --> RECV
    JR --> RECV
    CR --> RECV
    LU --> RECV
    LR --> RECV
    HB --> RECV

    CLEANUP2["Cleanup:<br/>- Broadcast 'user left'<br/>- room_remove_user()<br/>- remove_client()<br/>- close(fd)<br/>- free(client)"]
    CLEANUP2 --> END2([Thread Exit])

    style START fill:#6bcb77,color:#fff
    style END1 fill:#ff6b6b,color:#fff
    style END2 fill:#ff6b6b,color:#fff
    style CLEANUP2 fill:#ffd93d,color:#000
```

## 5. Authentication Flow

```mermaid
flowchart TD
    START([Auth Start]) --> CHOICE{"Login or<br/>Register?"}

    CHOICE -->|Register| REGCHECK["Check users.dat<br/>for username"]
    REGCHECK --> EXISTS{"Username<br/>exists?"}
    EXISTS -->|Yes| FAIL1["Send: 'Username taken'"]
    FAIL1 --> RETRY{"Attempts<br/>remaining?"}

    EXISTS -->|No| HASH1["DJB2 hash password"]
    HASH1 --> STORE["Append to users.dat"]
    STORE --> SUCCESS["Send: 'Registration OK'"]
    SUCCESS --> DONE([Authenticated ✓])

    CHOICE -->|Login| ONLINE{"Already<br/>online?"}
    ONLINE -->|Yes| FAIL2["Send: 'Already logged in'"]
    FAIL2 --> RETRY

    ONLINE -->|No| LOOKUP["Read users.dat<br/>Find username"]
    LOOKUP --> HASH2["DJB2 hash input password"]
    HASH2 --> MATCH{"Hash<br/>matches?"}
    MATCH -->|Yes| SUCCESS2["Send: 'Login OK'"]
    SUCCESS2 --> DONE

    MATCH -->|No| FAIL3["Send: 'Invalid credentials'"]
    FAIL3 --> RETRY

    RETRY -->|Yes| CHOICE
    RETRY -->|No| REJECT([Auth Failed ✗])

    style DONE fill:#6bcb77,color:#fff
    style REJECT fill:#ff6b6b,color:#fff
```

## 6. Memory Management Lifecycle

```mermaid
flowchart LR
    subgraph "Allocation"
        A1["accept() → new fd"]
        A2["calloc(1, sizeof client_t)"]
        A3["clients[i] = cl"]
    end

    subgraph "Usage"
        U1["Auth & join room"]
        U2["Message exchange"]
        U3["Room switches"]
    end

    subgraph "Deallocation"
        D1["remove_client(cl)"]
        D2["close(cl→sockfd)"]
        D3["free(cl)"]
    end

    A1 --> A2 --> A3 --> U1 --> U2 --> U3 --> D1 --> D2 --> D3

    style A2 fill:#ffd93d,color:#000
    style D3 fill:#ff6b6b,color:#fff
```

## 7. Data Structure Relationships

```mermaid
erDiagram
    SERVER ||--o{ CLIENT : "manages (MAX_CLIENTS=64)"
    SERVER ||--o{ ROOM : "contains (MAX_ROOMS=16)"
    SERVER ||--|| MSG_QUEUE : "has one"
    CLIENT }o--|| ROOM : "belongs to"
    CLIENT ||--o{ MESSAGE : "sends/receives"
    ROOM ||--o{ MESSAGE : "contains"

    CLIENT {
        int sockfd
        char addr
        char username
        char room
        int active
        pthread_t thread
    }

    ROOM {
        char name
        int active
        int user_count
    }

    MESSAGE {
        msg_type_t type
        char sender
        char recipient
        char room
        char body
        char timestamp
    }

    MSG_QUEUE {
        message_t messages_256
        int head
        int tail
        int count
        pthread_mutex_t mutex
    }
```

---

*Generated for Operating System Lab — Iqra University*
