# User Guide

## Real-Time Chat Application

| | |
|---|---|
| **Institution** | Iqra University |
| **Department** | Software Engineering |
| **Course** | Operating System Lab — Dynamic Data Structures |
| **Platform** | Fedora Linux |
| **Date** | June 2026 |

---

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [Installation](#2-installation)
3. [Compilation](#3-compilation)
4. [Starting the Server](#4-starting-the-server)
5. [Connecting a Client](#5-connecting-a-client)
6. [Authentication](#6-authentication)
7. [Command Reference](#7-command-reference)
8. [Usage Examples](#8-usage-examples)
9. [Troubleshooting](#9-troubleshooting)
10. [Frequently Asked Questions](#10-frequently-asked-questions)

---

## 1. Prerequisites

### 1.1 System Requirements

| Requirement | Minimum | Recommended |
|-------------|---------|-------------|
| **Operating System** | Fedora Linux 38+ | Fedora Linux 40 |
| **Kernel** | 5.x+ | 6.8+ |
| **Compiler** | GCC 12+ | GCC 14+ |
| **Build Tool** | GNU Make 4.x | GNU Make 4.4+ |
| **Libraries** | POSIX Threads (glibc) | — |
| **RAM** | 512 MB | 2 GB+ |
| **Terminal** | Any terminal supporting ANSI escape codes | GNOME Terminal / Kitty |

### 1.2 Installing Dependencies

On a fresh Fedora installation, ensure the following packages are installed:

```bash
# Update system packages
sudo dnf update -y

# Install GCC, Make, and development tools
sudo dnf install -y gcc make glibc-devel

# Verify installation
gcc --version
make --version
```

> [!NOTE]
> The POSIX Threads library (`pthread`) is part of the GNU C Library (`glibc`) and does not require a separate installation on Fedora.

### 1.3 Optional Tools

```bash
# For memory testing (recommended)
sudo dnf install -y valgrind

# For network inspection
sudo dnf install -y net-tools tcpdump
```

---

## 2. Installation

### 2.1 Obtaining the Source Code

Copy the project directory to your home folder or any working directory:

```bash
# If cloning from a repository
git clone https://github.com/RaahimAlavi/OS-Chat-Application
cd OS-Chat-Application

# Or if using a provided archive
tar -xzf OS-Chat-Application.tar.gz
cd OS-Chat-Application
```

### 2.2 Project Directory Structure

```
OS-Chat-Application/
├── Makefile                # Build system
├── docs/                   # Documentation
│   ├── system_design.md
│   ├── testing_documentation.md
│   ├── user_guide.md
│   └── extension_features.md
└── src/
    ├── common.h            # Shared definitions
    ├── auth.h              # Authentication module
    ├── server.c            # Server source code
    └── client.c            # Client source code
```

---

## 3. Compilation

### 3.1 Building Both Server and Client

```bash
make
```

**Expected output:**

```
gcc -Wall -Wextra -Wpedantic -std=c11 -g -O2 -o build/server src/server.c -pthread
✓ Server built: build/server
gcc -Wall -Wextra -Wpedantic -std=c11 -g -O2 -o build/client src/client.c -pthread
✓ Client built: build/client

✓ Build complete!
  Server: build/server
  Client: build/client

Quick start:
  Terminal 1:  build/server 9090
  Terminal 2:  build/client 127.0.0.1 9090
```

### 3.2 Building Individual Components

```bash
# Build only the server
make server

# Build only the client
make client
```

### 3.3 Cleaning Build Artifacts

```bash
make clean
```

This removes the `build/` directory and any `users.dat` credential file.

### 3.4 Available Make Targets

```bash
make help
```

| Target | Description |
|--------|-------------|
| `make` | Build both server and client |
| `make server` | Build server only |
| `make client` | Build client only |
| `make run-server` | Build and start the server (port 9090) |
| `make run-client` | Build and start a client (localhost:9090) |
| `make valgrind` | Run server under Valgrind for memory checking |
| `make clean` | Remove build artifacts and user database |
| `make help` | Display all available targets |

---

## 4. Starting the Server

### 4.1 Default Configuration

```bash
# Start server on default port (9090)
./build/server
```

### 4.2 Custom Port

```bash
# Start server on port 8080
./build/server 8080

# Or using make with PORT override
make run-server PORT=8080
```

### 4.3 Server Startup Output

```
╔═══════════════════════════════════════════════════╗
║      Real-Time Chat Server - Iqra University      ║
╠═══════════════════════════════════════════════════╣
║  Port:    9090                                    ║
║  Status:  LISTENING                               ║
║  Max:     64                                      ║
╚═══════════════════════════════════════════════════╝

[INFO] Server started. Waiting for connections...
Press Ctrl+C to shut down.
```

### 4.4 Server Log Messages

While running, the server prints colour-coded log messages:

| Prefix | Colour | Meaning |
|--------|--------|---------|
| `[INFO]` | Green | Successful operations (connections, logins, room creation) |
| `[WARN]` | Yellow | Non-critical issues (failed auth, max clients, shutdown) |
| `[ERROR]` | Red | Critical failures (socket errors, allocation failures) |
| `[DEBUG]` | Grey | Verbose message tracing (room broadcasts) |

### 4.5 Stopping the Server

Press **Ctrl+C** to initiate a graceful shutdown. The server will:

1. Set the `server_running` flag to `0`.
2. Notify all connected clients with `"Server is shutting down."`.
3. Close all client sockets.
4. Destroy the message queue.
5. Close the listening socket.
6. Exit cleanly.

---

## 5. Connecting a Client

### 5.1 Connecting to Localhost

```bash
# Default: connects to 127.0.0.1:9090
./build/client
```

### 5.2 Connecting to a Remote Server

```bash
# Connect to a specific IP and port
./build/client 192.168.1.100 8080

# Using make with overrides
make run-client SERVER_IP=192.168.1.100 PORT=8080
```

### 5.3 Client Startup Output

```
╔═══════════════════════════════════════════════════╗
║      Real-Time Chat Client - Iqra University      ║
╚═══════════════════════════════════════════════════╝

Connecting to 127.0.0.1:9090 ...
✓ Connected to server!
```

---

## 6. Authentication

### 6.1 Authentication Menu

Upon connecting, the client presents:

```
╔═══════════════════════════════════════════════════╗
║            Welcome to the Chat System!            ║
╠═══════════════════════════════════════════════════╣
║  1. Login                                         ║
║  2. Register                                      ║
╚═══════════════════════════════════════════════════╝

Choice (1/2):
```

### 6.2 Registering a New Account

```
Choice (1/2): 2
Username: alice
Password: mypassword123
✓ Registration successful. You are now logged in.
```

### 6.3 Logging In

```
Choice (1/2): 1
Username: alice
Password: mypassword123
✓ Login successful. Welcome back!
```

### 6.4 Authentication Errors

| Scenario | Error Message |
|----------|---------------|
| Wrong password | `✗ Invalid username or password.` |
| Duplicate username (registration) | `✗ Username already exists. Try a different one.` |
| Already logged in elsewhere | `✗ User is already logged in from another session.` |
| Connection lost | `Connection lost during authentication.` |

> [!IMPORTANT]
> After a failed authentication attempt, the menu is re-displayed so you can try again. The server allows **3 attempts** before disconnecting the client.

### 6.5 Post-Authentication

After successful login, you receive a welcome message and are placed in the default room:

```
*** Welcome, alice! You are in room [general]. Type /help for available commands. ***

╔═══════════════════════════════════════════════════════╗
║               Chat Application - Commands            ║
╠═══════════════════════════════════════════════════════╣
║  /msg <user> <text>   Send a private message         ║
║  /join <room>         Join or switch to a room       ║
║  /create <room>       Create a new room              ║
║  /rooms               List all available rooms       ║
║  /users               List all online users          ║
║  /clear               Clear the terminal screen      ║
║  /help                Show this help menu             ║
║  /quit                Disconnect and exit             ║
╠═══════════════════════════════════════════════════════╣
║  Any other text is sent as a message to your room.   ║
╚═══════════════════════════════════════════════════════╝

[general] >
```

---

## 7. Command Reference

### 7.1 Complete Command Table

| Command | Syntax | Description | Example |
|---------|--------|-------------|---------|
| **Send message** | `<text>` | Broadcasts a message to all users in your current room | `Hello everyone!` |
| **/msg** | `/msg <user> <text>` | Sends a private message to a specific online user | `/msg bob Hey, how are you?` |
| **/join** | `/join <room>` | Joins an existing room or creates and joins a new one | `/join dev-team` |
| **/create** | `/create <room>` | Creates a new chat room (without joining it) | `/create announcements` |
| **/rooms** | `/rooms` | Lists all available rooms with user counts | `/rooms` |
| **/users** | `/users` | Lists all currently online users and their rooms | `/users` |
| **/clear** | `/clear` | Clears the terminal screen | `/clear` |
| **/help** | `/help` | Displays the command help menu | `/help` |
| **/quit** | `/quit` | Gracefully disconnects and exits the client | `/quit` |

### 7.2 Message Types and Display Colours

| Message Type | Colour | Format |
|-------------|--------|--------|
| Broadcast (from others) | Blue (sender name) | `2026-06-15 18:30:01 alice: Hello!` |
| Broadcast (from you) | Green (sender name) | `2026-06-15 18:30:01 alice (you): Hello!` |
| Private message (received) | Magenta | `2026-06-15 18:30:01 [PM from bob]: Hi there` |
| Private message (sent) | Magenta | `2026-06-15 18:30:01 [PM to bob]: Hi there` |
| System notification | Yellow | `2026-06-15 18:30:01 *** bob has joined the chat! ***` |
| User list | Cyan border | Formatted box with usernames and rooms |
| Room list | Green border | Formatted box with room names and counts |
| Auth success | Green | `✓ Login successful.` |
| Auth failure | Red | `✗ Invalid username or password.` |

---

## 8. Usage Examples

### 8.1 Sample Chat Session

This example demonstrates a complete session with two users.

**Terminal 1 — Start the server:**

```bash
$ ./build/server 9090
```

**Terminal 2 — Alice connects and registers:**

```bash
$ ./build/client 127.0.0.1 9090
✓ Connected to server!

Choice (1/2): 2
Username: alice
Password: secret123
✓ Registration successful. You are now logged in.

*** Welcome, alice! You are in room [general]. Type /help for commands. ***

[general] > Hello, is anyone here?
2026-06-15 18:30:15 alice (you): Hello, is anyone here?
```

**Terminal 3 — Bob connects and logs in:**

```bash
$ ./build/client 127.0.0.1 9090
✓ Connected to server!

Choice (1/2): 2
Username: bob
Password: bobpass456
✓ Registration successful. You are now logged in.

*** Welcome, bob! You are in room [general]. Type /help for commands. ***

[general] >
```

**Alice's terminal now shows:**

```
2026-06-15 18:31:02 *** bob has joined the chat! [general] ***
[general] >
```

**Bob sends a message:**

```
[general] > Hi Alice! Let me create a room for our project.
2026-06-15 18:31:30 bob (you): Hi Alice! Let me create a room for our project.
[general] > /create os-project
*** Room 'os-project' created. ***
[general] > /join os-project
*** You joined room: os-project ***
[os-project] >
```

**Alice sends a private message to Bob:**

```
[general] > /msg bob I'll join your room in a moment!
2026-06-15 18:32:00 [PM to bob]: I'll join your room in a moment!
[general] > /rooms

╔══ Available Rooms ═══════════════════╗
  general              (1 users)
  os-project           (1 users)
╚══════════════════════════════════════╝

[general] > /join os-project
*** You joined room: os-project ***
[os-project] > Ready to discuss the project!
2026-06-15 18:32:30 alice (you): Ready to discuss the project!
```

**Bob sees:**

```
2026-06-15 18:32:00 [PM from alice]: I'll join your room in a moment!
[os-project] >
2026-06-15 18:32:15 *** alice joined the room. ***
2026-06-15 18:32:30 alice: Ready to discuss the project!
[os-project] >
```

### 8.2 Listing Users

```
[general] > /users

╔══ Online Users ══════════════════════╗
  alice                [os-project]
  bob                  [os-project]
  charlie              [general]
╚══════════════════════════════════════╝
```

### 8.3 Disconnecting

```
[os-project] > /quit

Disconnecting...
Goodbye!
```

---

## 9. Troubleshooting

### 9.1 Common Issues

| Problem | Likely Cause | Solution |
|---------|-------------|----------|
| `connect() failed: Connection refused` | Server is not running or wrong port | Start the server first; verify port number |
| `bind() failed: Address already in use` | Previous server instance didn't shut down cleanly | Wait 30 seconds, or use `SO_REUSEADDR` (already enabled) |
| `*** Disconnected from server ***` | Server shut down or network interrupted | Restart the server and reconnect |
| `Server is full. Try again later.` | 64 clients already connected | Wait for a client to disconnect |
| No colour output in terminal | Terminal does not support ANSI codes | Use GNOME Terminal, Kitty, or Alacritty |
| `Authentication failed. Exiting.` | Exceeded 3 failed login attempts | Restart client; verify username/password |
| `Permission denied` on `users.dat` | File permission issue | Run `chmod 644 users.dat` |
| Build fails: `pthread.h: No such file` | Missing development headers | Run `sudo dnf install glibc-devel` |

### 9.2 Checking if the Server is Running

```bash
# Method 1: Check process list
ps aux | grep server

# Method 2: Check if port is in use
ss -tlnp | grep 9090
```

### 9.3 Resetting User Database

To clear all registered users and start fresh:

```bash
rm -f users.dat
# Or use: make clean
```

### 9.4 Debugging Connection Issues

```bash
# Test if the port is reachable
nc -zv 127.0.0.1 9090

# Check firewall rules (if connecting over LAN)
sudo firewall-cmd --list-ports
sudo firewall-cmd --add-port=9090/tcp --permanent
sudo firewall-cmd --reload
```

---

## 10. Frequently Asked Questions

### General

**Q: What is the maximum number of users that can connect simultaneously?**
A: The server supports up to **64 concurrent clients**, as defined by the `MAX_CLIENTS` constant in `common.h`. This can be increased by modifying the constant and recompiling.

**Q: Can I run the server and clients on different machines?**
A: Yes. Start the server on one machine and specify the server's IP address when launching the client:
```bash
./build/client 192.168.1.100 9090
```
Ensure that port 9090 (or your chosen port) is open in the firewall.

**Q: What happens if I lose my internet connection while chatting?**
A: The client will detect the disconnection and display `*** Disconnected from server ***`. The server will automatically clean up your session. Other users will see a notification that you have left.

### Authentication

**Q: Can I change my password?**
A: Password changes are not supported in the current version. To reset a password, delete the `users.dat` file and re-register. Note that this deletes all user accounts.

**Q: Is my password stored securely?**
A: Passwords are hashed using the DJB2 algorithm before storage. However, DJB2 is **not** a cryptographic hash function and should not be considered secure for production use. This implementation is for educational purposes.

### Rooms

**Q: What is the maximum number of rooms?**
A: Up to **16 rooms** can exist simultaneously, as defined by `MAX_ROOMS`.

**Q: Can I delete a room?**
A: Room deletion is not supported in the current version. Rooms persist until the server is restarted.

**Q: What is the default room?**
A: All users are automatically placed in the `general` room upon login.

### Messages

**Q: Are messages stored or logged?**
A: Messages are pushed to an internal broadcast queue for logging purposes, but no persistent message log is written to disk. Messages exist only in memory during the server's lifetime.

**Q: Can I send messages to users in different rooms?**
A: Regular broadcast messages are only visible to users in your current room. However, **private messages** (`/msg`) can be sent to any online user regardless of their room.

**Q: Is there a message length limit?**
A: Messages can be up to **2047 characters** (one less than `BUFFER_SIZE` to allow for null termination).

---

> [!TIP]
> For technical details about the system architecture, refer to the [System Design Document](system_design.md). For information about extended features, see [Extension Features](extension_features.md).
