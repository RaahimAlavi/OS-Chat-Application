# Final Presentation — Real-Time Chat Application

> **Operating System Lab | Iqra University | Department of Software Engineering**
> *Dynamic Data Structures: Building a Real-Time Chat Application*

---

## Slide 1: Title

**Real-Time Chat Application Using TCP/IP Socket Programming in C**

- Course: Operating System (Lab)
- Institution: Iqra University, Department of Software Engineering
- Topic: Dynamic Data Structures
- Platform: Fedora Linux

---

## Slide 2: Problem Statement

### Objective
Build a multi-user real-time chat application that:
- Connects users over a local network using TCP/IP sockets
- Handles multiple simultaneous client connections
- Ensures reliable message delivery without data loss
- Manages connections dynamically with proper memory management

### Key Concepts
- Socket Programming | Inter-Process Communication | Concurrency Control | Dynamic Memory

---

## Slide 3: System Architecture

```
                    ┌─────────────────────┐
                    │    CHAT SERVER       │
                    │                     │
                    │  ┌───────────────┐  │
                    │  │ Accept Loop   │  │
                    │  │ (Main Thread) │  │
                    │  └──────┬────────┘  │
                    │         │           │
                    │  ┌──────┴────────┐  │
                    │  │ Thread Pool   │  │
                    │  │ T1 │ T2 │ TN  │  │
                    │  └──────────────┘  │
                    │         │           │
                    │  ┌──────┴────────┐  │
                    │  │  Shared State │  │
                    │  │  • Clients[]  │  │
                    │  │  • Rooms[]    │  │
                    │  │  • MsgQueue   │  │
                    │  │  • Auth DB    │  │
                    │  └───────────────┘  │
                    └──────┬──────────────┘
                           │ TCP/IP
              ┌────────────┼────────────┐
              │            │            │
         ┌────┴────┐ ┌────┴────┐ ┌────┴────┐
         │Client 1 │ │Client 2 │ │Client N │
         │(2 thds) │ │(2 thds) │ │(2 thds) │
         └─────────┘ └─────────┘ └─────────┘
```

---

## Slide 4: Core Technologies

| Technology | Purpose |
|---|---|
| **C Language** | System-level programming, direct memory control |
| **POSIX Threads** | Concurrent client handling (one thread per client) |
| **TCP Sockets** | Reliable, ordered byte-stream communication |
| **Mutex Locks** | Thread-safe access to shared client/room arrays |
| **Dynamic Memory** | `calloc()`/`free()` for client lifecycle management |
| **Signal Handling** | `SIGINT`/`SIGTERM` for graceful server shutdown |

---

## Slide 5: Key Data Structures

### client_t — Client Session
```c
typedef struct {
    int       sockfd;              // Socket file descriptor
    char      addr[INET_ADDRSTRLEN]; // Client IP
    char      username[32];        // Authenticated username
    char      room[32];            // Current chat room
    int       active;              // Connection status
    pthread_t thread;              // Handler thread
} client_t;
```

### msg_queue_t — Thread-Safe Circular Buffer
```c
typedef struct {
    message_t       messages[256]; // Fixed-size ring buffer
    int             head, tail, count;
    pthread_mutex_t mutex;         // Protects concurrent access
    pthread_cond_t  cond;          // Signals new messages
} msg_queue_t;
```

---

## Slide 6: Message Protocol

| Type | Direction | Purpose |
|---|---|---|
| `MSG_AUTH_REQUEST` | Client → Server | Login/register credentials |
| `MSG_AUTH_RESPONSE` | Server → Client | Authentication result |
| `MSG_BROADCAST` | Bidirectional | Public room message |
| `MSG_PRIVATE` | Client → Server → Client | Direct message |
| `MSG_SYSTEM` | Server → Client | Notifications (join/leave) |
| `MSG_JOIN_ROOM` | Client → Server | Switch rooms |
| `MSG_LIST_USERS` | Client → Server | Request online users |
| `MSG_LIST_ROOMS` | Client → Server | Request room list |
| `MSG_DISCONNECT` | Client → Server | Graceful disconnect |

---

## Slide 7: Extension Features

### ✅ User Authentication
- DJB2 hash-based password storage
- File-based credential persistence (`users.dat`)
- Login and registration flow

### ✅ Private Messaging
- `/msg <user> <message>` command
- Direct delivery without room broadcast

### ✅ Multi-Room Chat
- Dynamic room creation (`/create <name>`)
- Room switching (`/join <name>`)
- User count tracking per room

### ✅ Color-Coded Terminal UI
- ANSI escape codes for message differentiation
- Timestamps, system alerts, PM indicators

---

## Slide 8: Memory Management

```
CONNECT                    ACTIVE                    DISCONNECT
  │                          │                          │
  ▼                          ▼                          ▼
calloc(client_t)   ──►  Read/Write msgs   ──►    remove_client()
add_client()             Room switches           close(sockfd)
room_add_user()          Private msgs            free(client_t)
```

### Key Principles
- **Allocation**: `calloc()` on each new connection
- **Protection**: `pthread_mutex_t` on shared arrays
- **Deallocation**: `free()` on disconnect, verified with Valgrind
- **No Leaks**: Every `calloc()` has a matching `free()`

---

## Slide 9: Testing Results

| Test Category | Tests | Pass Rate |
|---|---|---|
| Connection | 4 | 100% |
| Authentication | 3 | 100% |
| Messaging | 4 | 100% |
| Room Management | 3 | 100% |
| Edge Cases | 3 | 100% |
| **Total** | **17** | **100%** |

### Performance Metrics
- Connection time: < 5ms
- Message latency: < 2ms (localhost)
- Throughput: ~1000 msg/sec
- Memory per client: ~152 bytes
- Valgrind: **0 memory leaks**

---

## Slide 10: Demo

### Quick Start
```bash
# Terminal 1: Start Server
./build/server 9090

# Terminal 2: Client A
./build/client 127.0.0.1 9090

# Terminal 3: Client B
./build/client 127.0.0.1 9090
```

### Demo Flow
1. Register two users → Login
2. Send broadcast messages in `#general`
3. Create room `#project-os`, join with one user
4. Send private message `/msg user2 Hello!`
5. List users and rooms
6. Graceful disconnect

---

## Slide 11: Concluding Remarks

### Achievements
- ✅ Full TCP/IP chat server with concurrent client handling
- ✅ Dynamic memory management with zero leaks
- ✅ Thread-safe shared data structures
- ✅ User authentication and multi-room support
- ✅ Private messaging and real-time broadcast
- ✅ Comprehensive testing and documentation

### Future Enhancements
- 🔒 TLS/SSL encryption for secure communication
- 🖥️ GUI client (GTK/ncurses)
- 🗄️ SQLite database for user and message persistence
- 📁 File transfer support
- 🌐 IPv6 and DNS hostname resolution

---

## Slide 12: Q&A

**Thank You!**

*Questions and Discussion*

---

*Operating System Lab — Iqra University — Department of Software Engineering*
