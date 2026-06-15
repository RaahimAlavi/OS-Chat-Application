/*
 * ============================================================================
 * Real-Time Chat Application - Common Definitions
 * ============================================================================
 * Department of Software Engineering, Iqra University
 * Operating System Lab - Dynamic Data Structures
 *
 * This header defines shared constants, data structures, and utility macros
 * used by both the chat server and client components.
 * ============================================================================
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/* ----------------------------- Configuration ----------------------------- */
#define DEFAULT_PORT        9090
#define MAX_CLIENTS         64
#define MAX_ROOMS           16
#define BUFFER_SIZE         2048
#define USERNAME_LEN        32
#define PASSWORD_LEN        64
#define ROOM_NAME_LEN       32
#define MSG_QUEUE_SIZE      256
#define TIMESTAMP_LEN       32
#define DEFAULT_ROOM        "general"

/* ----------------------------- Message Types ----------------------------- */
typedef enum {
    MSG_AUTH_REQUEST,       /* Client -> Server: login/register request       */
    MSG_AUTH_RESPONSE,      /* Server -> Client: authentication result        */
    MSG_BROADCAST,          /* Public message to all users in a room          */
    MSG_PRIVATE,            /* Private message between two users              */
    MSG_SYSTEM,             /* System notification (join/leave/etc.)          */
    MSG_JOIN_ROOM,          /* Client requests to join a room                 */
    MSG_LEAVE_ROOM,         /* Client requests to leave a room                */
    MSG_LIST_USERS,         /* Client requests online user list               */
    MSG_LIST_ROOMS,         /* Client requests available rooms list           */
    MSG_USER_LIST,          /* Server -> Client: list of users                */
    MSG_ROOM_LIST,          /* Server -> Client: list of rooms                */
    MSG_CREATE_ROOM,        /* Client requests to create a new room           */
    MSG_DISCONNECT,         /* Graceful disconnect notification               */
    MSG_HEARTBEAT           /* Keep-alive ping/pong                           */
} msg_type_t;

/* ----------------------------- Auth Actions ------------------------------ */
typedef enum {
    AUTH_LOGIN,
    AUTH_REGISTER
} auth_action_t;

/* ----------------------------- Message Struct ---------------------------- */
typedef struct {
    msg_type_t  type;                       /* Message type                   */
    char        sender[USERNAME_LEN];       /* Who sent the message           */
    char        recipient[USERNAME_LEN];    /* Target user (private msgs)     */
    char        room[ROOM_NAME_LEN];        /* Target room                    */
    char        body[BUFFER_SIZE];          /* Message content                */
    char        timestamp[TIMESTAMP_LEN];   /* ISO-formatted timestamp        */
    auth_action_t auth_action;              /* Login or Register              */
    int         status;                     /* 0 = failure, 1 = success       */
} message_t;

/* ----------------------------- Color Codes ------------------------------- */
#define CLR_RESET   "\033[0m"
#define CLR_RED     "\033[1;31m"
#define CLR_GREEN   "\033[1;32m"
#define CLR_YELLOW  "\033[1;33m"
#define CLR_BLUE    "\033[1;34m"
#define CLR_MAGENTA "\033[1;35m"
#define CLR_CYAN    "\033[1;36m"
#define CLR_WHITE   "\033[1;37m"
#define CLR_GRAY    "\033[0;90m"
#define CLR_BOLD    "\033[1m"

/* ----------------------------- Utility Macros ---------------------------- */
#define LOG_INFO(fmt, ...)  \
    fprintf(stdout, CLR_GREEN "[INFO] " CLR_RESET fmt "\n", ##__VA_ARGS__)

#define LOG_WARN(fmt, ...)  \
    fprintf(stdout, CLR_YELLOW "[WARN] " CLR_RESET fmt "\n", ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    fprintf(stderr, CLR_RED "[ERROR] " CLR_RESET fmt "\n", ##__VA_ARGS__)

#define LOG_DEBUG(fmt, ...) \
    fprintf(stdout, CLR_GRAY "[DEBUG] " CLR_RESET fmt "\n", ##__VA_ARGS__)

/* ----------------------------- Utility Functions ------------------------- */

/**
 * get_timestamp - Writes the current time as an ISO-8601 string.
 * @buf:  Destination buffer (must be >= TIMESTAMP_LEN).
 * @len:  Size of the destination buffer.
 */
static inline void get_timestamp(char *buf, size_t len)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, len, "%Y-%m-%d %H:%M:%S", tm_info);
}

/**
 * trim_newline - Removes trailing newline/carriage-return characters.
 * @str:  Null-terminated string to trim in place.
 */
static inline void trim_newline(char *str)
{
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r'))
        str[--len] = '\0';
}

/**
 * send_message - Serializes and sends a message_t over a socket.
 * @sockfd:  The connected socket file descriptor.
 * @msg:     Pointer to the message structure.
 * Returns:  Number of bytes sent, or -1 on error.
 */
static inline ssize_t send_message(int sockfd, const message_t *msg)
{
    return send(sockfd, msg, sizeof(message_t), 0);
}

/**
 * recv_message - Receives and deserializes a message_t from a socket.
 * @sockfd:  The connected socket file descriptor.
 * @msg:     Pointer to the destination message structure.
 * Returns:  Number of bytes received, 0 on disconnect, or -1 on error.
 */
static inline ssize_t recv_message(int sockfd, message_t *msg)
{
    ssize_t total = 0;
    ssize_t bytes_left = sizeof(message_t);
    char *ptr = (char *)msg;

    while (bytes_left > 0) {
        ssize_t n = recv(sockfd, ptr + total, bytes_left, 0);
        if (n <= 0)
            return n;
        total += n;
        bytes_left -= n;
    }
    return total;
}

#endif /* COMMON_H */
