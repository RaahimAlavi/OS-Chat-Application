# ============================================================================
# OS-Chat-Application - Makefile
# ============================================================================
# Department of Software Engineering, Iqra University
# Operating System Lab - Dynamic Data Structures
#
# Usage:
#   make            Build both server and client
#   make server     Build only the server
#   make client     Build only the client
#   make clean      Remove compiled binaries
#   make run-server Run the server (default port 9090)
#   make run-client Run the client (connects to localhost:9090)
#   make valgrind   Run server with Valgrind to check for memory leaks
#   make help       Show available targets
# ============================================================================

CC          = gcc
CFLAGS      = -Wall -Wextra -Wpedantic -std=c11 -g -O2
LDFLAGS     = -pthread
SRC_DIR     = src
BUILD_DIR   = build

SERVER_SRC  = $(SRC_DIR)/server.c
CLIENT_SRC  = $(SRC_DIR)/client.c
REPORT_SRC  = $(SRC_DIR)/report_generator.c
HEADERS     = $(SRC_DIR)/common.h $(SRC_DIR)/auth.h

SERVER_BIN  = $(BUILD_DIR)/server
CLIENT_BIN  = $(BUILD_DIR)/client
REPORT_BIN  = $(BUILD_DIR)/report_gen

PORT        ?= 9090
SERVER_IP   ?= 127.0.0.1

# ---- Default Target ----
.PHONY: all
all: $(SERVER_BIN) $(CLIENT_BIN) $(REPORT_BIN)
	@echo ""
	@echo "\033[1;32m✓ Build complete!\033[0m"
	@echo "  Server:     $(SERVER_BIN)"
	@echo "  Client:     $(CLIENT_BIN)"
	@echo "  Report Gen: $(REPORT_BIN)"
	@echo ""
	@echo "Quick start:"
	@echo "  Terminal 1:  $(SERVER_BIN) $(PORT)"
	@echo "  Terminal 2:  $(CLIENT_BIN) $(SERVER_IP) $(PORT)"
	@echo ""

# ---- Build Directory ----
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ---- Server ----
.PHONY: server
server: $(SERVER_BIN)

$(SERVER_BIN): $(SERVER_SRC) $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "\033[1;32m✓ Server built: $@\033[0m"

# ---- Client ----
.PHONY: client
client: $(CLIENT_BIN)

$(CLIENT_BIN): $(CLIENT_SRC) $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "\033[1;32m✓ Client built: $@\033[0m"

# ---- Report Generator ----
.PHONY: report
report: $(REPORT_BIN)

$(REPORT_BIN): $(REPORT_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $<
	@echo "\033[1;32m✓ Report generator built: $@\033[0m"

# ---- Run Targets ----
.PHONY: run-server
run-server: $(SERVER_BIN)
	@echo "Starting server on port $(PORT)..."
	./$(SERVER_BIN) $(PORT)

.PHONY: run-client
run-client: $(CLIENT_BIN)
	@echo "Connecting to $(SERVER_IP):$(PORT)..."
	./$(CLIENT_BIN) $(SERVER_IP) $(PORT)

# ---- Testing and Debugging ----
.PHONY: valgrind
valgrind: $(SERVER_BIN)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
		./$(SERVER_BIN) $(PORT)

.PHONY: test
test: all
	@echo "Running basic connectivity test..."
	@echo "Start server in one terminal: make run-server"
	@echo "Start client(s) in other terminals: make run-client"

# ---- Cleanup ----
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) users.dat
	@echo "\033[1;33m✓ Clean complete.\033[0m"

# ---- Help ----
.PHONY: help
help:
	@echo ""
	@echo "\033[1;36m═══ Chat Application Build System ═══\033[0m"
	@echo ""
	@echo "  make              Build server and client"
	@echo "  make server       Build server only"
	@echo "  make client       Build client only"
	@echo "  make run-server   Start the server (PORT=$(PORT))"
	@echo "  make run-client   Start a client (SERVER_IP=$(SERVER_IP) PORT=$(PORT))"
	@echo "  make valgrind     Run server with Valgrind"
	@echo "  make clean        Remove build artifacts"
	@echo "  make help         Show this help"
	@echo ""
	@echo "  Override defaults:  make run-server PORT=8080"
	@echo "                      make run-client SERVER_IP=192.168.1.10 PORT=8080"
	@echo ""
