/*
 * ============================================================================
 * OS-Chat-Application - Server
 * ============================================================================
 * Department of Software Engineering, Iqra University
 * Operating System Lab - Dynamic Data Structures
 *
 * Multi-threaded TCP chat server supporting:
 *   - User authentication (login / register)
 *   - Multi-room chat with dynamic room creation
 *   - Private messaging between users
 *   - Real-time broadcast within rooms
 *   - Dynamic memory management for clients and message queues
 *   - Graceful shutdown with resource cleanup
 *
 * Compile:  gcc -Wall -Wextra -pthread -o server server.c
 * Usage:    ./server [port]          (default port: 9090)
 * ============================================================================
 */

#include "common.h"
#include "auth.h"

/* ========================== Data Structures ============================== */

/**
 * struct client_t - Represents a connected client session.
 * @sockfd:     Client socket file descriptor.
 * @addr:       Client IP address (dotted-quad string).
 * @username:   Authenticated username.
 * @room:       Currently joined chat room.
 * @active:     1 if the client is connected, 0 otherwise.
 * @thread:     POSIX thread handling this client.
 */
typedef struct {
    int             sockfd;
    char            addr[INET_ADDRSTRLEN];
    char            username[USERNAME_LEN];
    char            room[ROOM_NAME_LEN];
    int             active;
    pthread_t       thread;
} client_t;

/**
 * struct room_t - Represents a chat room.
 * @name:       Room name (unique identifier).
 * @active:     1 if the room exists, 0 otherwise.
 * @user_count: Number of users currently in the room.
 */
typedef struct {
    char    name[ROOM_NAME_LEN];
    int     active;
    int     user_count;
} room_t;

/**
 * struct msg_queue_t - Thread-safe circular message queue.
 * @messages:   Fixed-size array of messages.
 * @head:       Index of the next message to dequeue.
 * @tail:       Index of the next empty slot.
 * @count:      Number of messages currently in the queue.
 * @mutex:      Protects concurrent access.
 * @cond:       Signals when a new message is available.
 */
typedef struct {
    message_t       messages[MSG_QUEUE_SIZE];
    int             head;
    int             tail;
    int             count;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
} msg_queue_t;

/* ========================== Global State ================================ */

static client_t    *clients[MAX_CLIENTS];        /* Connected client slots   */
static room_t       rooms[MAX_ROOMS];            /* Chat room registry       */
static msg_queue_t  broadcast_queue;             /* Global broadcast queue   */
static int          server_fd       = -1;        /* Listening socket         */
static int          client_count    = 0;         /* Active client count      */
static int          room_count      = 0;         /* Active room count        */
static volatile int server_running  = 1;         /* Shutdown flag            */

static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t rooms_mutex   = PTHREAD_MUTEX_INITIALIZER;

/* ========================== Event Logging ================================ */

#define LOG_FILE "server.log"

static FILE            *log_fp      = NULL;
static pthread_mutex_t  log_mutex   = PTHREAD_MUTEX_INITIALIZER;

/**
 * log_event - Writes a structured CSV event line to server.log.
 * Format: TIMESTAMP,EVENT,USERNAME,ROOM,DETAILS
 *
 * Event types:
 *   CONNECT, DISCONNECT, AUTH_SUCCESS, AUTH_FAIL, REGISTER,
 *   MSG_BROADCAST, MSG_PRIVATE, ROOM_JOIN, ROOM_LEAVE,
 *   ROOM_CREATE, SERVER_START, SERVER_STOP
 */
static void log_event(const char *event, const char *username,
                      const char *room, const char *details)
{
    if (!log_fp) return;

    char ts[TIMESTAMP_LEN];
    get_timestamp(ts, TIMESTAMP_LEN);

    pthread_mutex_lock(&log_mutex);
    fprintf(log_fp, "%s,%s,%s,%s,%s\n",
            ts,
            event     ? event     : "",
            username  ? username  : "",
            room      ? room      : "",
            details   ? details   : "");
    fflush(log_fp);
    pthread_mutex_unlock(&log_mutex);
}

/* ========================== Message Queue ================================ */

/**
 * queue_init - Initializes the message queue.
 */
static void queue_init(msg_queue_t *q)
{
    memset(q, 0, sizeof(msg_queue_t));
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);
}

/**
 * queue_push - Enqueues a message (drops oldest if full).
 */
static void queue_push(msg_queue_t *q, const message_t *msg)
{
    pthread_mutex_lock(&q->mutex);
    q->messages[q->tail] = *msg;
    q->tail = (q->tail + 1) % MSG_QUEUE_SIZE;
    if (q->count == MSG_QUEUE_SIZE)
        q->head = (q->head + 1) % MSG_QUEUE_SIZE;   /* Overwrite oldest */
    else
        q->count++;
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->mutex);
}

/**
 * queue_destroy - Cleans up queue resources.
 */
static void queue_destroy(msg_queue_t *q)
{
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond);
}

/* ========================== Room Management ============================== */

/**
 * create_room - Creates a new chat room.
 * @name:  Room name.
 * Returns: 1 on success, 0 if room exists or limit reached.
 */
static int create_room(const char *name)
{
    pthread_mutex_lock(&rooms_mutex);

    /* Check if room already exists */
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active && strcmp(rooms[i].name, name) == 0) {
            pthread_mutex_unlock(&rooms_mutex);
            return 0;
        }
    }

    /* Find an empty slot */
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (!rooms[i].active) {
            strncpy(rooms[i].name, name, ROOM_NAME_LEN - 1);
            rooms[i].name[ROOM_NAME_LEN - 1] = '\0';
            rooms[i].active = 1;
            rooms[i].user_count = 0;
            room_count++;
            LOG_INFO("Room created: %s", name);
            pthread_mutex_unlock(&rooms_mutex);
            return 1;
        }
    }

    pthread_mutex_unlock(&rooms_mutex);
    return 0;   /* No room slots available */
}

/**
 * room_add_user / room_remove_user - Adjusts user count.
 */
static void room_add_user(const char *name)
{
    pthread_mutex_lock(&rooms_mutex);
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active && strcmp(rooms[i].name, name) == 0) {
            rooms[i].user_count++;
            break;
        }
    }
    pthread_mutex_unlock(&rooms_mutex);
}

static void room_remove_user(const char *name)
{
    pthread_mutex_lock(&rooms_mutex);
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active && strcmp(rooms[i].name, name) == 0) {
            if (rooms[i].user_count > 0)
                rooms[i].user_count--;
            break;
        }
    }
    pthread_mutex_unlock(&rooms_mutex);
}

/* ========================== Client Management ============================ */

/**
 * add_client - Registers a new client in the global array.
 */
static void add_client(client_t *cl)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i]) {
            clients[i] = cl;
            client_count++;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

/**
 * remove_client - Unregisters a client from the global array and frees memory.
 */
static void remove_client(client_t *cl)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == cl) {
            clients[i] = NULL;
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

/**
 * find_client_by_name - Looks up a client by username.
 * Returns: Pointer to the client_t, or NULL.
 * NOTE: Must be called with clients_mutex held.
 */
static client_t *find_client_by_name(const char *username)
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->active &&
            strcmp(clients[i]->username, username) == 0)
            return clients[i];
    }
    return NULL;
}

/**
 * is_username_online - Checks if a username is already connected.
 */
static int is_username_online(const char *username)
{
    pthread_mutex_lock(&clients_mutex);
    client_t *cl = find_client_by_name(username);
    pthread_mutex_unlock(&clients_mutex);
    return cl != NULL;
}

/* ========================== Message Delivery ============================= */

/**
 * broadcast_to_room - Sends a message to all clients in a specific room.
 * @msg:           The message to broadcast.
 * @exclude_fd:    Socket fd to exclude (the sender), or -1 for none.
 */
static void broadcast_to_room(const message_t *msg, int exclude_fd)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->active &&
            clients[i]->sockfd != exclude_fd &&
            strcmp(clients[i]->room, msg->room) == 0) {
            send_message(clients[i]->sockfd, msg);
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    /* Also push to the global queue for logging */
    queue_push(&broadcast_queue, msg);
}

/**
 * broadcast_to_all - Sends a message to every connected client.
 * @msg:           The message to broadcast.
 * @exclude_fd:    Socket fd to exclude, or -1 for none.
 */
static void broadcast_to_all(const message_t *msg, int exclude_fd)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->active &&
            clients[i]->sockfd != exclude_fd) {
            send_message(clients[i]->sockfd, msg);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

/**
 * send_system_msg - Convenience: sends a system notification to one client.
 */
static void send_system_msg(int sockfd, const char *text)
{
    message_t msg;
    memset(&msg, 0, sizeof(message_t));
    msg.type = MSG_SYSTEM;
    strncpy(msg.sender, "SERVER", USERNAME_LEN - 1);
    strncpy(msg.body, text, BUFFER_SIZE - 1);
    get_timestamp(msg.timestamp, TIMESTAMP_LEN);
    send_message(sockfd, &msg);
}

/* ========================== Authentication =============================== */

/**
 * handle_auth - Processes login/register for a new client.
 * Returns: 1 on success, 0 on failure.
 */
static int handle_auth(client_t *cl)
{
    message_t msg;
    message_t resp;
    int attempts = 3;

    while (attempts-- > 0) {
        ssize_t n = recv_message(cl->sockfd, &msg);
        if (n <= 0)
            return 0;

        if (msg.type != MSG_AUTH_REQUEST)
            continue;

        memset(&resp, 0, sizeof(message_t));
        resp.type = MSG_AUTH_RESPONSE;
        strncpy(resp.sender, "SERVER", USERNAME_LEN - 1);
        get_timestamp(resp.timestamp, TIMESTAMP_LEN);

        if (msg.auth_action == AUTH_REGISTER) {
            int result = auth_register(msg.sender, msg.body);
            if (result == 1) {
                resp.status = 1;
                strncpy(resp.body, "Registration successful. You are now logged in.", BUFFER_SIZE - 1);
                strncpy(cl->username, msg.sender, USERNAME_LEN - 1);
                send_message(cl->sockfd, &resp);
                LOG_INFO("User registered: %s", cl->username);
                return 1;
            } else if (result == 0) {
                resp.status = 0;
                strncpy(resp.body, "Username already exists. Try a different one.", BUFFER_SIZE - 1);
                send_message(cl->sockfd, &resp);
            } else {
                resp.status = 0;
                strncpy(resp.body, "Server error during registration.", BUFFER_SIZE - 1);
                send_message(cl->sockfd, &resp);
            }
        } else if (msg.auth_action == AUTH_LOGIN) {
            /* Check if user is already online */
            if (is_username_online(msg.sender)) {
                resp.status = 0;
                strncpy(resp.body, "User is already logged in from another session.", BUFFER_SIZE - 1);
                send_message(cl->sockfd, &resp);
                continue;
            }

            int result = auth_login(msg.sender, msg.body);
            if (result == 1) {
                resp.status = 1;
                strncpy(resp.body, "Login successful. Welcome back!", BUFFER_SIZE - 1);
                strncpy(cl->username, msg.sender, USERNAME_LEN - 1);
                send_message(cl->sockfd, &resp);
                LOG_INFO("User logged in: %s", cl->username);
                return 1;
            } else {
                resp.status = 0;
                strncpy(resp.body, "Invalid username or password.", BUFFER_SIZE - 1);
                send_message(cl->sockfd, &resp);
            }
        }
    }
    return 0;
}

/* ========================== Command Handlers ============================= */

/**
 * handle_list_users - Sends the list of online users to the requester.
 */
static void handle_list_users(client_t *cl)
{
    message_t resp;
    memset(&resp, 0, sizeof(message_t));
    resp.type = MSG_USER_LIST;
    strncpy(resp.sender, "SERVER", USERNAME_LEN - 1);
    get_timestamp(resp.timestamp, TIMESTAMP_LEN);

    char list[BUFFER_SIZE] = "";
    int offset = 0;

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && clients[i]->active) {
            int written = snprintf(list + offset, BUFFER_SIZE - offset,
                                   "  %-20s [%s]\n",
                                   clients[i]->username, clients[i]->room);
            if (written > 0)
                offset += written;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    if (offset == 0)
        strncpy(resp.body, "  No users online.", BUFFER_SIZE - 1);
    else
        strncpy(resp.body, list, BUFFER_SIZE - 1);

    send_message(cl->sockfd, &resp);
}

/**
 * handle_list_rooms - Sends the list of available rooms to the requester.
 */
static void handle_list_rooms(client_t *cl)
{
    message_t resp;
    memset(&resp, 0, sizeof(message_t));
    resp.type = MSG_ROOM_LIST;
    strncpy(resp.sender, "SERVER", USERNAME_LEN - 1);
    get_timestamp(resp.timestamp, TIMESTAMP_LEN);

    char list[BUFFER_SIZE] = "";
    int offset = 0;

    pthread_mutex_lock(&rooms_mutex);
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (rooms[i].active) {
            int written = snprintf(list + offset, BUFFER_SIZE - offset,
                                   "  %-20s (%d users)\n",
                                   rooms[i].name, rooms[i].user_count);
            if (written > 0)
                offset += written;
        }
    }
    pthread_mutex_unlock(&rooms_mutex);

    if (offset == 0)
        strncpy(resp.body, "  No rooms available.", BUFFER_SIZE - 1);
    else
        strncpy(resp.body, list, BUFFER_SIZE - 1);

    send_message(cl->sockfd, &resp);
}

/**
 * handle_join_room - Moves a client to a different room.
 */
static void handle_join_room(client_t *cl, const char *room_name)
{
    char old_room[ROOM_NAME_LEN];
    strncpy(old_room, cl->room, ROOM_NAME_LEN - 1);
    old_room[ROOM_NAME_LEN - 1] = '\0';

    /* Don't rejoin the same room */
    if (strcmp(old_room, room_name) == 0) {
        send_system_msg(cl->sockfd, "You are already in that room.");
        return;
    }

    /* Leave current room */
    room_remove_user(old_room);

    /* Notify old room — clearly state where the user went */
    message_t sys;
    memset(&sys, 0, sizeof(message_t));
    sys.type = MSG_SYSTEM;
    strncpy(sys.sender, "SERVER", USERNAME_LEN - 1);
    strncpy(sys.room, old_room, ROOM_NAME_LEN - 1);
    snprintf(sys.body, BUFFER_SIZE, "%s switched to room [%s].",
             cl->username, room_name);
    get_timestamp(sys.timestamp, TIMESTAMP_LEN);
    broadcast_to_room(&sys, cl->sockfd);

    /* Log the room leave */
    log_event("ROOM_LEAVE", cl->username, old_room, room_name);

    /* Create room if it doesn't exist */
    create_room(room_name);

    /* Update client room */
    strncpy(cl->room, room_name, ROOM_NAME_LEN - 1);
    cl->room[ROOM_NAME_LEN - 1] = '\0';
    room_add_user(room_name);

    /* Notify new room */
    strncpy(sys.room, room_name, ROOM_NAME_LEN - 1);
    snprintf(sys.body, BUFFER_SIZE, "%s joined the room (from [%s]).",
             cl->username, old_room);
    get_timestamp(sys.timestamp, TIMESTAMP_LEN);
    broadcast_to_room(&sys, cl->sockfd);

    /* Log the room join */
    log_event("ROOM_JOIN", cl->username, room_name, old_room);

    /* Confirm to user */
    char confirm[BUFFER_SIZE];
    snprintf(confirm, BUFFER_SIZE, "You joined room: %s", room_name);
    send_system_msg(cl->sockfd, confirm);
}

/**
 * handle_private_msg - Delivers a private message to a specific user.
 */
static void handle_private_msg(client_t *sender, const message_t *msg)
{
    pthread_mutex_lock(&clients_mutex);
    client_t *target = find_client_by_name(msg->recipient);
    if (target) {
        send_message(target->sockfd, msg);
        pthread_mutex_unlock(&clients_mutex);
    } else {
        pthread_mutex_unlock(&clients_mutex);
        send_system_msg(sender->sockfd, "User not found or offline.");
    }
}

/* ========================== Client Handler Thread ======================== */

/**
 * client_handler - Main thread function for each connected client.
 *
 * Handles authentication, then enters a receive loop dispatching messages
 * by type.  On disconnect, cleans up all resources.
 */
static void *client_handler(void *arg)
{
    client_t *cl = (client_t *)arg;
    message_t msg;

    LOG_INFO("Client connected: %s (fd=%d)", cl->addr, cl->sockfd);
    log_event("CONNECT", "", "", cl->addr);

    /* --- Authentication Phase --- */
    if (!handle_auth(cl)) {
        LOG_WARN("Authentication failed for %s", cl->addr);
        log_event("AUTH_FAIL", "", "", cl->addr);
        close(cl->sockfd);
        free(cl);
        return NULL;
    }

    /* --- Join Default Room --- */
    cl->active = 1;
    add_client(cl);

    strncpy(cl->room, DEFAULT_ROOM, ROOM_NAME_LEN - 1);
    room_add_user(DEFAULT_ROOM);

    /* Announce join */
    memset(&msg, 0, sizeof(message_t));
    msg.type = MSG_SYSTEM;
    strncpy(msg.sender, "SERVER", USERNAME_LEN - 1);
    strncpy(msg.room, cl->room, ROOM_NAME_LEN - 1);
    snprintf(msg.body, BUFFER_SIZE, "%s has joined the chat! [%s]",
             cl->username, cl->room);
    get_timestamp(msg.timestamp, TIMESTAMP_LEN);
    broadcast_to_room(&msg, cl->sockfd);

    /* Welcome message */
    char welcome[BUFFER_SIZE];
    snprintf(welcome, BUFFER_SIZE,
             "Welcome, %s! You are in room [%s]. "
             "Type /help for available commands.",
             cl->username, cl->room);
    send_system_msg(cl->sockfd, welcome);

    LOG_INFO("User %s authenticated and joined [%s]", cl->username, cl->room);
    log_event("AUTH_SUCCESS", cl->username, cl->room, cl->addr);

    /* --- Message Receive Loop --- */
    while (server_running) {
        ssize_t n = recv_message(cl->sockfd, &msg);
        if (n <= 0) {
            LOG_INFO("Client %s disconnected (fd=%d)", cl->username, cl->sockfd);
            break;
        }

        switch (msg.type) {
        case MSG_BROADCAST:
            /* Fill in server-side metadata */
            strncpy(msg.sender, cl->username, USERNAME_LEN - 1);
            strncpy(msg.room, cl->room, ROOM_NAME_LEN - 1);
            get_timestamp(msg.timestamp, TIMESTAMP_LEN);

            LOG_DEBUG("[%s] %s: %s", msg.room, msg.sender, msg.body);
            log_event("MSG_BROADCAST", cl->username, cl->room, msg.body);
            broadcast_to_room(&msg, cl->sockfd);
            break;

        case MSG_PRIVATE:
            strncpy(msg.sender, cl->username, USERNAME_LEN - 1);
            get_timestamp(msg.timestamp, TIMESTAMP_LEN);
            log_event("MSG_PRIVATE", cl->username, "", msg.recipient);
            handle_private_msg(cl, &msg);
            break;

        case MSG_JOIN_ROOM:
            handle_join_room(cl, msg.room);
            break;

        case MSG_CREATE_ROOM:
            if (create_room(msg.room)) {
                log_event("ROOM_CREATE", cl->username, msg.room, "");
                char buf[BUFFER_SIZE];
                snprintf(buf, BUFFER_SIZE, "Room '%s' created.", msg.room);
                send_system_msg(cl->sockfd, buf);
            } else {
                send_system_msg(cl->sockfd, "Room already exists or limit reached.");
            }
            break;

        case MSG_LIST_USERS:
            handle_list_users(cl);
            break;

        case MSG_LIST_ROOMS:
            handle_list_rooms(cl);
            break;

        case MSG_DISCONNECT:
            goto cleanup;

        case MSG_HEARTBEAT:
            send_message(cl->sockfd, &msg);   /* Pong */
            break;

        default:
            LOG_WARN("Unknown message type %d from %s", msg.type, cl->username);
            break;
        }
    }

cleanup:
    /* --- Cleanup --- */
    cl->active = 0;

    /* Announce departure */
    memset(&msg, 0, sizeof(message_t));
    msg.type = MSG_SYSTEM;
    strncpy(msg.sender, "SERVER", USERNAME_LEN - 1);
    strncpy(msg.room, cl->room, ROOM_NAME_LEN - 1);
    snprintf(msg.body, BUFFER_SIZE, "%s has left the chat.", cl->username);
    log_event("DISCONNECT", cl->username, cl->room, "");
    get_timestamp(msg.timestamp, TIMESTAMP_LEN);
    broadcast_to_room(&msg, -1);

    room_remove_user(cl->room);
    remove_client(cl);

    close(cl->sockfd);
    LOG_INFO("Client %s cleaned up and freed (fd=%d)", cl->username, cl->sockfd);
    free(cl);

    return NULL;
}

/* ========================== Signal Handler =============================== */

static void signal_handler(int sig)
{
    (void)sig;
    LOG_WARN("Shutting down server...");
    server_running = 0;
    if (server_fd != -1)
        close(server_fd);
}

/* ========================== Main ========================================= */

int main(int argc, char *argv[])
{
    int port = DEFAULT_PORT;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    if (argc > 1)
        port = atoi(argv[1]);

    /* Install signal handlers for graceful shutdown */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);          /* Ignore broken pipe */

    /* Initialize global state */
    memset(clients, 0, sizeof(clients));
    memset(rooms, 0, sizeof(rooms));
    queue_init(&broadcast_queue);
    create_room(DEFAULT_ROOM);          /* Always create the default room */

    /* Open event log file */
    log_fp = fopen(LOG_FILE, "a");
    if (!log_fp) {
        LOG_WARN("Could not open %s for logging (reports will be empty)", LOG_FILE);
    } else {
        log_event("SERVER_START", "", "", "port");
    }

    /* Create listening socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        LOG_ERROR("socket() failed: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    /* Allow immediate port reuse */
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Bind */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR("bind() failed: %s", strerror(errno));
        close(server_fd);
        return EXIT_FAILURE;
    }

    /* Listen */
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        LOG_ERROR("listen() failed: %s", strerror(errno));
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("\n");
    printf(CLR_CYAN "╔═══════════════════════════════════════════════════╗\n");
    printf("║      Real-Time Chat Server - Iqra University      ║\n");
    printf("╠═══════════════════════════════════════════════════╣\n");
    printf("║  Port:    %-38d ║\n", port);
    printf("║  Status:  %-38s ║\n", "LISTENING");
    printf("║  Max:     %-38d ║\n", MAX_CLIENTS);
    printf("╚═══════════════════════════════════════════════════╝\n" CLR_RESET);
    printf("\n");
    LOG_INFO("Server started. Waiting for connections...");
    printf("Press Ctrl+C to shut down.\n\n");

    /* --- Accept Loop --- */
    while (server_running) {
        int client_fd = accept(server_fd,
                               (struct sockaddr *)&client_addr,
                               &addr_len);
        if (client_fd < 0) {
            if (!server_running) break;   /* Shutdown triggered */
            LOG_ERROR("accept() failed: %s", strerror(errno));
            continue;
        }

        /* Check capacity */
        if (client_count >= MAX_CLIENTS) {
            LOG_WARN("Max clients reached. Rejecting connection.");
            send_system_msg(client_fd, "Server is full. Try again later.");
            close(client_fd);
            continue;
        }

        /* Allocate client structure (dynamic memory) */
        client_t *cl = (client_t *)calloc(1, sizeof(client_t));
        if (!cl) {
            LOG_ERROR("calloc() failed for new client");
            close(client_fd);
            continue;
        }

        cl->sockfd = client_fd;
        cl->active = 0;  /* Will be set to 1 after authentication */
        inet_ntop(AF_INET, &client_addr.sin_addr, cl->addr, INET_ADDRSTRLEN);

        /* Spawn client handler thread (detached) */
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

        if (pthread_create(&cl->thread, &attr, client_handler, cl) != 0) {
            LOG_ERROR("pthread_create() failed: %s", strerror(errno));
            close(client_fd);
            free(cl);
        }

        pthread_attr_destroy(&attr);
    }

    /* --- Server Shutdown --- */
    LOG_INFO("Closing all client connections...");
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i]) {
            send_system_msg(clients[i]->sockfd, "Server is shutting down.");
            close(clients[i]->sockfd);
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    log_event("SERVER_STOP", "", "", "");
    queue_destroy(&broadcast_queue);
    if (log_fp) fclose(log_fp);
    close(server_fd);

    LOG_INFO("Server shut down cleanly.");
    return EXIT_SUCCESS;
}
