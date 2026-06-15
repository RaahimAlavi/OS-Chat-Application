# OS-Chat-Application

**Repository:** [https://github.com/RaahimAlavi/OS-Chat-Application](https://github.com/RaahimAlavi/OS-Chat-Application)

> **Operating System Lab — Iqra University**
> Dynamic Data Structures: Building a Real-Time Chat Application

---

## Overview

A multi-user real-time chat application built in **C** using **TCP/IP socket programming** on **Fedora Linux**. The system demonstrates core operating system concepts including:

- **Dynamic Memory Allocation** — Client structures allocated/freed on connect/disconnect
- **Process/Thread Management** — POSIX threads for concurrent client handling
- **Inter-Process Communication** — TCP sockets for network-based message exchange
- **Concurrency Control** — Mutex-protected shared data structures
- **Real-Time Data Handling** — Low-latency message broadcast and delivery

## Architecture

```
┌─────────────────────────────────────────────────┐
│                  CHAT SERVER                     │
│                                                  │
│  ┌──────────┐  ┌──────────┐  ┌──────────────┐  │
│  │   Auth    │  │  Room    │  │   Message    │  │
│  │  Module   │  │ Manager  │  │    Queue     │  │
│  └──────────┘  └──────────┘  └──────────────┘  │
│                                                  │
│  ┌──────────────────────────────────────────┐   │
│  │        Thread Pool (per-client)           │   │
│  │  [Thread 1] [Thread 2] ... [Thread N]    │   │
│  └──────────────────────────────────────────┘   │
│                                                  │
│  ┌──────────────────────────────────────────┐   │
│  │        TCP Listening Socket (:9090)       │   │
│  └──────────────────────────────────────────┘   │
└──────────────────┬──────────────────────────────┘
                   │ TCP/IP
    ┌──────────────┼──────────────┐
    │              │              │
┌───┴───┐    ┌────┴────┐   ┌────┴────┐
│Client 1│    │Client 2 │   │Client N │
└────────┘    └─────────┘   └─────────┘
```

## Features

| Feature | Description |
|---------|-------------|
| **User Authentication** | Login/Register with DJB2 password hashing |
| **Multi-Room Chat** | Create, join, and switch between rooms |
| **Private Messaging** | Direct messages between online users |
| **Real-Time Broadcast** | Instant message delivery to room members |
| **Color-Coded UI** | ANSI color-coded terminal interface |
| **Thread-Safe Queues** | Circular buffer message queue with mutex protection |
| **Graceful Shutdown** | Signal-handler based cleanup (SIGINT/SIGTERM) |
| **Report Generator** | Performance and usage report generation |

## Quick Start

### Prerequisites

```bash
# Fedora Linux
sudo dnf install gcc make valgrind
```

### Build

```bash
make            # Build server, client, and report generator
```

### Run

```bash
# Terminal 1 — Start the server
make run-server

# Terminal 2 — Connect a client
make run-client

# Terminal 3 — Connect another client
make run-client
```

### Generate Report

```bash
./build/report_gen                    # Print to stdout
./build/report_gen report.txt         # Save to file
```

## Client Commands

| Command | Description |
|---------|-------------|
| `/msg <user> <text>` | Send a private message |
| `/join <room>` | Join/switch to a chat room |
| `/create <room>` | Create a new room |
| `/rooms` | List all available rooms |
| `/users` | List all online users |
| `/clear` | Clear the terminal screen |
| `/help` | Show command help |
| `/quit` | Disconnect and exit |

## Project Structure

```
OS-Chat-Application/
├── src/
│   ├── common.h            # Shared definitions, protocol, utilities
│   ├── auth.h              # Authentication module (DJB2 hash)
│   ├── server.c            # Multi-threaded TCP chat server
│   ├── client.c            # Interactive terminal chat client
│   └── report_generator.c  # Performance report generator
├── docs/
│   ├── system_design.md         # System design & block diagrams
│   ├── testing_documentation.md # Test plan, cases & results
│   ├── user_guide.md            # Complete user guide
│   └── extension_features.md   # Extension features documentation
├── Makefile                # Build system
└── README.md               # This file
```

## Configuration

| Parameter | Default | Environment Variable |
|-----------|---------|---------------------|
| Port | 9090 | `PORT` |
| Server IP | 127.0.0.1 | `SERVER_IP` |
| Max Clients | 64 | Compile-time (`common.h`) |
| Max Rooms | 16 | Compile-time (`common.h`) |
| Buffer Size | 2048 | Compile-time (`common.h`) |

## Testing

```bash
# Memory leak check with Valgrind
make valgrind

# Custom port
make run-server PORT=8080
make run-client PORT=8080

# Remote connection
make run-client SERVER_IP=192.168.1.100 PORT=9090
```

## Documentation

- [System Design Document](docs/system_design.md)
- [Testing Documentation](docs/testing_documentation.md)
- [User Guide](docs/user_guide.md)
- [Extension Features](docs/extension_features.md)

## Authors

Department of Software Engineering, Iqra University

## License

This project is developed for educational purposes as part of the Operating System Lab course.
