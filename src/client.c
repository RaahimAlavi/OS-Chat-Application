#include "common.h"

//Global State

static int              sockfd          = -1;
static char             username[USERNAME_LEN] = "";
static char             current_room[ROOM_NAME_LEN] = DEFAULT_ROOM;
static volatile int     connected       = 1;
static pthread_t        recv_thread;

//Display Helpers

//clear_input_line
static void clear_input_line(void)
{
    printf("\r\033[K");
}

//print_prompt displays the input prompt with room context.
static void print_prompt(void)
{
    printf(CLR_CYAN "[%s]" CLR_RESET " > ", current_room);
    fflush(stdout);
}

//display_message - Renders a received message with colors and timestamps.
static void display_message(const message_t *msg)
{
    clear_input_line();

    switch (msg->type) {
    case MSG_BROADCAST:
        printf(CLR_GRAY "%s " CLR_BOLD CLR_BLUE "%s" CLR_RESET ": %s\n",
               msg->timestamp, msg->sender, msg->body);
        break;

    case MSG_PRIVATE:
        printf(CLR_GRAY "%s " CLR_MAGENTA "[PM from %s]" CLR_RESET ": %s\n",
               msg->timestamp, msg->sender, msg->body);
        break;

    case MSG_SYSTEM:
        printf(CLR_GRAY "%s " CLR_YELLOW "*** %s ***" CLR_RESET "\n",
               msg->timestamp, msg->body);
        break;

    case MSG_USER_LIST:
        printf(CLR_CYAN "\n╔══ Online Users ══════════════════════╗\n" CLR_RESET);
        printf("%s", msg->body);
        printf(CLR_CYAN "╚══════════════════════════════════════╝\n" CLR_RESET);
        break;

    case MSG_ROOM_LIST:
        printf(CLR_GREEN "\n╔══ Available Rooms ═══════════════════╗\n" CLR_RESET);
        printf("%s", msg->body);
        printf(CLR_GREEN "╚══════════════════════════════════════╝\n" CLR_RESET);
        break;

    case MSG_AUTH_RESPONSE:
        if (msg->status)
            printf(CLR_GREEN "✓ %s" CLR_RESET "\n", msg->body);
        else
            printf(CLR_RED "✗ %s" CLR_RESET "\n", msg->body);
        break;

    default:
        printf(CLR_GRAY "[%s] %s" CLR_RESET "\n", msg->sender, msg->body);
        break;
    }

    print_prompt();
}

//Help Menu

static void print_help(void)
{
    printf(CLR_CYAN "\n");
    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║               Chat Application - Commands            ║\n");
    printf("╠═══════════════════════════════════════════════════════╣\n");
    printf("║  /msg <user> <text>   Send a private message         ║\n");
    printf("║  /join <room>         Join or switch to a room       ║\n");
    printf("║  /create <room>       Create a new room              ║\n");
    printf("║  /rooms               List all available rooms       ║\n");
    printf("║  /users               List all online users          ║\n");
    printf("║  /clear               Clear the terminal screen      ║\n");
    printf("║  /help                Show this help menu             ║\n");
    printf("║  /quit                Disconnect and exit             ║\n");
    printf("╠═══════════════════════════════════════════════════════╣\n");
    printf("║  Any other text is sent as a message to your room.   ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n" CLR_RESET);
    printf("\n");
}

//Receive Thread

//receive_handler - Runs in a separate thread, continuously receiving messages from the server and displaying them.
static void *receive_handler(void *arg)
{
    (void)arg;
    message_t msg;

    while (connected) {
        ssize_t n = recv_message(sockfd, &msg);
        if (n <= 0) {
            if (connected) {
                clear_input_line();
                printf(CLR_RED "\n*** Disconnected from server ***\n" CLR_RESET);
                connected = 0;
            }
            break;
        }

        /* Update local room state if we got a join confirmation */
        if (msg.type == MSG_SYSTEM && strstr(msg.body, "You joined room:")) {
            const char *rname = msg.body + strlen("You joined room: ");
            strncpy(current_room, rname, ROOM_NAME_LEN - 1);
            current_room[ROOM_NAME_LEN - 1] = '\0';
        }

        display_message(&msg);
    }

    return NULL;
}

//Command Parsing

//process_input - Parses user input and dispatches the appropriate message.
static void process_input(const char *input)
{
    message_t msg;
    memset(&msg, 0, sizeof(message_t));

    if (strlen(input) == 0)
        return;

    //quit
    if (strcmp(input, "/quit") == 0 || strcmp(input, "/exit") == 0) {
        msg.type = MSG_DISCONNECT;
        strncpy(msg.sender, username, USERNAME_LEN - 1);
        send_message(sockfd, &msg);
        connected = 0;
        return;
    }

    //help
    if (strcmp(input, "/help") == 0) {
        print_help();
        return;
    }

    //clear
    if (strcmp(input, "/clear") == 0) {
        printf("\033[2J\033[H");   /* ANSI clear screen + home */
        return;
    }

    //users
    if (strcmp(input, "/users") == 0) {
        msg.type = MSG_LIST_USERS;
        send_message(sockfd, &msg);
        return;
    }

    //rooms
    if (strcmp(input, "/rooms") == 0) {
        msg.type = MSG_LIST_ROOMS;
        send_message(sockfd, &msg);
        return;
    }

    //join <room>
    if (strncmp(input, "/join ", 6) == 0) {
        const char *room = input + 6;
        while (*room == ' ') room++;
        if (strlen(room) == 0) {
            printf(CLR_RED "Usage: /join <room_name>\n" CLR_RESET);
            return;
        }
        msg.type = MSG_JOIN_ROOM;
        strncpy(msg.room, room, ROOM_NAME_LEN - 1);
        send_message(sockfd, &msg);
        return;
    }

    //create <room>
    if (strncmp(input, "/create ", 8) == 0) {
        const char *room = input + 8;
        while (*room == ' ') room++;
        if (strlen(room) == 0) {
            printf(CLR_RED "Usage: /create <room_name>\n" CLR_RESET);
            return;
        }
        msg.type = MSG_CREATE_ROOM;
        strncpy(msg.room, room, ROOM_NAME_LEN - 1);
        send_message(sockfd, &msg);
        return;
    }

    //msg <user> <text>
    if (strncmp(input, "/msg ", 5) == 0) {
        char target[USERNAME_LEN] = "";
        const char *p = input + 5;
        while (*p == ' ') p++;

        //Extract target username
        int i = 0;
        while (*p && *p != ' ' && i < USERNAME_LEN - 1)
            target[i++] = *p++;
        target[i] = '\0';

        //Skip space after username
        while (*p == ' ') p++;

        if (strlen(target) == 0 || strlen(p) == 0) {
            printf(CLR_RED "Usage: /msg <username> <message>\n" CLR_RESET);
            return;
        }

        msg.type = MSG_PRIVATE;
        strncpy(msg.sender, username, USERNAME_LEN - 1);
        strncpy(msg.recipient, target, USERNAME_LEN - 1);
        strncpy(msg.body, p, BUFFER_SIZE - 1);
        get_timestamp(msg.timestamp, TIMESTAMP_LEN);
        send_message(sockfd, &msg);

        //Show local echo
        printf(CLR_GRAY "%s " CLR_MAGENTA "[PM to %s]" CLR_RESET ": %s\n",
               msg.timestamp, target, p);
        return;
    }

    //Unknown command
    if (input[0] == '/') {
        printf(CLR_RED "Unknown command: %s. Type /help for help.\n" CLR_RESET,
               input);
        return;
    }

    //Regular broadcast message
    msg.type = MSG_BROADCAST;
    strncpy(msg.sender, username, USERNAME_LEN - 1);
    strncpy(msg.room, current_room, ROOM_NAME_LEN - 1);
    strncpy(msg.body, input, BUFFER_SIZE - 1);
    get_timestamp(msg.timestamp, TIMESTAMP_LEN);
    send_message(sockfd, &msg);

    //Local echo
    printf(CLR_GRAY "%s " CLR_BOLD CLR_GREEN "%s (you)" CLR_RESET ": %s\n",
           msg.timestamp, username, input);
}

//Authentication

//authenticate - Handles the login/register flow interactively. Returns: 1 on success, 0 on failure.
static int authenticate(void)
{
    char input[BUFFER_SIZE];
    char password[PASSWORD_LEN];
    message_t msg, resp;

    printf(CLR_CYAN "\n╔═══════════════════════════════════════════════════╗\n");
    printf("║            Welcome to the Chat System!            ║\n");
    printf("╠═══════════════════════════════════════════════════╣\n");
    printf("║  1. Login                                         ║\n");
    printf("║  2. Register                                      ║\n");
    printf("╚═══════════════════════════════════════════════════╝\n" CLR_RESET);

    while (1) {
        printf("\nChoice (1/2): ");
        fflush(stdout);
        if (!fgets(input, sizeof(input), stdin))
            return 0;
        trim_newline(input);

        auth_action_t action;
        if (strcmp(input, "1") == 0)
            action = AUTH_LOGIN;
        else if (strcmp(input, "2") == 0)
            action = AUTH_REGISTER;
        else {
            printf(CLR_RED "Please enter 1 or 2.\n" CLR_RESET);
            continue;
        }

        printf("Username: ");
        fflush(stdout);
        if (!fgets(input, sizeof(input), stdin))
            return 0;
        trim_newline(input);

        if (strlen(input) == 0 || strlen(input) >= USERNAME_LEN) {
            printf(CLR_RED "Invalid username (1-%d chars).\n" CLR_RESET,
                   USERNAME_LEN - 1);
            continue;
        }

        printf("Password: ");
        fflush(stdout);
        if (!fgets(password, sizeof(password), stdin))
            return 0;
        trim_newline(password);

        if (strlen(password) == 0) {
            printf(CLR_RED "Password cannot be empty.\n" CLR_RESET);
            continue;
        }

        //Build auth request
        memset(&msg, 0, sizeof(message_t));
        msg.type = MSG_AUTH_REQUEST;
        msg.auth_action = action;
        strncpy(msg.sender, input, USERNAME_LEN - 1);
        strncpy(msg.body, password, BUFFER_SIZE - 1);

        send_message(sockfd, &msg);

        //Wait for response
        ssize_t n = recv_message(sockfd, &resp);
        if (n <= 0) {
            printf(CLR_RED "Connection lost during authentication.\n" CLR_RESET);
            return 0;
        }

        if (resp.type == MSG_AUTH_RESPONSE) {
            if (resp.status == 1) {
                strncpy(username, input, USERNAME_LEN - 1);
                printf(CLR_GREEN "✓ %s\n" CLR_RESET, resp.body);
                return 1;
            } else {
                printf(CLR_RED "✗ %s\n" CLR_RESET, resp.body);
                //Loop back to try again
            }
        }
    }
}

//Signal Handler

static void signal_handler(int sig)
{
    (void)sig;
    connected = 0;
    if (sockfd != -1)
        close(sockfd);
}

//Main

int main(int argc, char *argv[])
{
    const char *server_ip = "127.0.0.1";
    int port = DEFAULT_PORT;
    struct sockaddr_in server_addr;
    char input[BUFFER_SIZE];

    if (argc > 1)
        server_ip = argv[1];
    if (argc > 2)
        port = atoi(argv[2]);

    /* Install signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    //Display banner
    printf(CLR_CYAN "\n");
    printf("╔═══════════════════════════════════════════════════╗\n");
    printf("║      Real-Time Chat Client - Iqra University      ║\n");
    printf("╚═══════════════════════════════════════════════════╝\n" CLR_RESET);
    printf("\nConnecting to %s:%d ...\n", server_ip, port);

    //Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LOG_ERROR("socket() failed: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    //Connect to server
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        LOG_ERROR("Invalid server address: %s", server_ip);
        close(sockfd);
        return EXIT_FAILURE;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR("connect() failed: %s", strerror(errno));
        close(sockfd);
        return EXIT_FAILURE;
    }

    printf(CLR_GREEN "✓ Connected to server!\n" CLR_RESET);

    //Authenticate
    if (!authenticate()) {
        printf(CLR_RED "Authentication failed. Exiting.\n" CLR_RESET);
        close(sockfd);
        return EXIT_FAILURE;
    }

    //Start receive thread
    if (pthread_create(&recv_thread, NULL, receive_handler, NULL) != 0) {
        LOG_ERROR("Failed to create receive thread");
        close(sockfd);
        return EXIT_FAILURE;
    }

    //Print help
    print_help();

    //Main Input Loop
    while (connected) {
        print_prompt();

        if (!fgets(input, sizeof(input), stdin)) {
            if (connected) {
                printf("\n");
                connected = 0;
            }
            break;
        }

        trim_newline(input);

        if (strlen(input) == 0)
            continue;

        process_input(input);
    }

    //Cleanup
    printf(CLR_YELLOW "\nDisconnecting...\n" CLR_RESET);
    close(sockfd);
    pthread_cancel(recv_thread);

    printf(CLR_GREEN "Goodbye!\n" CLR_RESET);
    return EXIT_SUCCESS;
}
