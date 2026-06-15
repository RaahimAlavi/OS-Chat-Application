/*
 * ============================================================================
 * OS-Chat-Application - Report Generator
 * ============================================================================
 * Department of Software Engineering, Iqra University
 * Operating System Lab - Dynamic Data Structures
 *
 * Parses the server's event log (server.log) and generates a structured
 * performance and usage report with real data.
 *
 * Log format (CSV):  TIMESTAMP,EVENT,USERNAME,ROOM,DETAILS
 *
 * Compile:  gcc -Wall -Wextra -o report_gen report_generator.c
 * Usage:    ./report_gen [server.log] [output_file]
 *           Defaults: reads server.log, prints to stdout
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ========================== Configuration ================================ */

#define MAX_LINE        4096
#define MAX_USERS       256
#define MAX_ROOMS       64
#define USERNAME_LEN    32
#define ROOM_NAME_LEN   32
#define DETAIL_LEN      2048
#define TIMESTAMP_LEN   32

/* ========================== Data Structures ============================== */

typedef struct {
    char    username[USERNAME_LEN];
    int     messages_sent;
    int     private_msgs_sent;
    int     rooms_joined;
    int     login_count;
    char    first_seen[TIMESTAMP_LEN];
    char    last_seen[TIMESTAMP_LEN];
} user_stats_t;

typedef struct {
    char    name[ROOM_NAME_LEN];
    int     total_messages;
    int     joins;
    int     leaves;
} room_stats_t;

typedef struct {
    /* General */
    char    log_file[256];
    int     total_lines;

    /* Connection stats */
    int     total_connections;
    int     total_disconnections;
    int     auth_success;
    int     auth_fail;

    /* Message stats */
    long    broadcast_messages;
    long    private_messages;
    long    total_messages;

    /* Room stats */
    int     rooms_created;

    /* Server lifecycle */
    int     server_starts;
    int     server_stops;
    char    first_event[TIMESTAMP_LEN];
    char    last_event[TIMESTAMP_LEN];

    /* Users */
    user_stats_t users[MAX_USERS];
    int          user_count;

    /* Rooms */
    room_stats_t rooms[MAX_ROOMS];
    int          room_count;
} report_t;

/* ========================== Lookup Helpers ================================ */

/**
 * find_or_add_user - Finds a user by name, or creates a new entry.
 */
static user_stats_t *find_or_add_user(report_t *r, const char *username)
{
    if (!username || username[0] == '\0')
        return NULL;

    for (int i = 0; i < r->user_count; i++) {
        if (strcmp(r->users[i].username, username) == 0)
            return &r->users[i];
    }

    if (r->user_count >= MAX_USERS)
        return NULL;

    user_stats_t *u = &r->users[r->user_count++];
    memset(u, 0, sizeof(user_stats_t));
    strncpy(u->username, username, USERNAME_LEN - 1);
    return u;
}

/**
 * find_or_add_room - Finds a room by name, or creates a new entry.
 */
static room_stats_t *find_or_add_room(report_t *r, const char *room)
{
    if (!room || room[0] == '\0')
        return NULL;

    for (int i = 0; i < r->room_count; i++) {
        if (strcmp(r->rooms[i].name, room) == 0)
            return &r->rooms[i];
    }

    if (r->room_count >= MAX_ROOMS)
        return NULL;

    room_stats_t *rm = &r->rooms[r->room_count++];
    memset(rm, 0, sizeof(room_stats_t));
    strncpy(rm->name, room, ROOM_NAME_LEN - 1);
    return rm;
}

/* ========================== CSV Parser =================================== */

/**
 * parse_csv_field - Extracts the next CSV field from *ptr, advancing past
 *                   the delimiter.  Returns the start of the field.
 */
static char *parse_csv_field(char **ptr)
{
    if (!ptr || !*ptr)
        return NULL;

    char *start = *ptr;
    char *comma = strchr(start, ',');

    if (comma) {
        *comma = '\0';
        *ptr = comma + 1;
    } else {
        /* Last field — trim newline */
        size_t len = strlen(start);
        while (len > 0 && (start[len - 1] == '\n' || start[len - 1] == '\r'))
            start[--len] = '\0';
        *ptr = NULL;
    }

    return start;
}

/**
 * parse_log_file - Reads and parses server.log, populating the report struct.
 * Returns: number of lines parsed, or -1 on error.
 */
static int parse_log_file(const char *filename, report_t *r)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open log file: %s\n", filename);
        return -1;
    }

    strncpy(r->log_file, filename, sizeof(r->log_file) - 1);

    char line[MAX_LINE];
    int line_count = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_count++;

        char *ptr = line;
        char *timestamp = parse_csv_field(&ptr);
        char *event     = parse_csv_field(&ptr);
        char *username  = parse_csv_field(&ptr);
        char *room      = parse_csv_field(&ptr);
        char *details   = parse_csv_field(&ptr);

        if (!timestamp || !event)
            continue;

        /* Track first/last timestamps */
        if (r->first_event[0] == '\0')
            strncpy(r->first_event, timestamp, TIMESTAMP_LEN - 1);
        strncpy(r->last_event, timestamp, TIMESTAMP_LEN - 1);

        /* ---- Dispatch by event type ---- */

        if (strcmp(event, "CONNECT") == 0) {
            r->total_connections++;
        }
        else if (strcmp(event, "DISCONNECT") == 0) {
            r->total_disconnections++;
            user_stats_t *u = find_or_add_user(r, username);
            if (u)
                strncpy(u->last_seen, timestamp, TIMESTAMP_LEN - 1);
        }
        else if (strcmp(event, "AUTH_SUCCESS") == 0) {
            r->auth_success++;
            user_stats_t *u = find_or_add_user(r, username);
            if (u) {
                u->login_count++;
                if (u->first_seen[0] == '\0')
                    strncpy(u->first_seen, timestamp, TIMESTAMP_LEN - 1);
                strncpy(u->last_seen, timestamp, TIMESTAMP_LEN - 1);
            }
            /* User joins default room on auth */
            if (room && room[0] != '\0') {
                room_stats_t *rm = find_or_add_room(r, room);
                if (rm) rm->joins++;
                if (u) u->rooms_joined++;
            }
        }
        else if (strcmp(event, "AUTH_FAIL") == 0) {
            r->auth_fail++;
        }
        else if (strcmp(event, "MSG_BROADCAST") == 0) {
            r->broadcast_messages++;
            r->total_messages++;
            user_stats_t *u = find_or_add_user(r, username);
            if (u) {
                u->messages_sent++;
                strncpy(u->last_seen, timestamp, TIMESTAMP_LEN - 1);
            }
            room_stats_t *rm = find_or_add_room(r, room);
            if (rm) rm->total_messages++;
        }
        else if (strcmp(event, "MSG_PRIVATE") == 0) {
            r->private_messages++;
            r->total_messages++;
            user_stats_t *u = find_or_add_user(r, username);
            if (u) {
                u->private_msgs_sent++;
                u->messages_sent++;
                strncpy(u->last_seen, timestamp, TIMESTAMP_LEN - 1);
            }
        }
        else if (strcmp(event, "ROOM_JOIN") == 0) {
            user_stats_t *u = find_or_add_user(r, username);
            if (u) u->rooms_joined++;
            room_stats_t *rm = find_or_add_room(r, room);
            if (rm) rm->joins++;
        }
        else if (strcmp(event, "ROOM_LEAVE") == 0) {
            room_stats_t *rm = find_or_add_room(r, room);
            if (rm) rm->leaves++;
        }
        else if (strcmp(event, "ROOM_CREATE") == 0) {
            r->rooms_created++;
            find_or_add_room(r, room);
        }
        else if (strcmp(event, "SERVER_START") == 0) {
            r->server_starts++;
        }
        else if (strcmp(event, "SERVER_STOP") == 0) {
            r->server_stops++;
        }
    }

    r->total_lines = line_count;
    fclose(fp);
    return line_count;
}

/* ========================== Report Output ================================ */

static void print_separator(FILE *out, char c, int width)
{
    for (int i = 0; i < width; i++)
        fputc(c, out);
    fputc('\n', out);
}

static void print_header(FILE *out, const char *title, int width)
{
    int len = (int)strlen(title);
    int pad = (width - len - 2) / 2;
    fputc('\n', out);
    print_separator(out, '=', width);
    for (int i = 0; i < pad; i++) fputc(' ', out);
    fprintf(out, " %s ", title);
    fputc('\n', out);
    print_separator(out, '=', width);
}

static void generate_report(FILE *out, const report_t *r)
{
    int W = 70;

    /* ---- Title ---- */
    print_header(out, "OS-CHAT-APPLICATION - SERVER REPORT", W);
    fprintf(out, "\n");
    fprintf(out, "  Institution:   Iqra University\n");
    fprintf(out, "  Department:    Software Engineering\n");
    fprintf(out, "  Course:        Operating System (Lab)\n");
    fprintf(out, "  Log File:      %s\n", r->log_file);
    fprintf(out, "  Log Lines:     %d\n", r->total_lines);
    fprintf(out, "  Period:        %s  to  %s\n",
            r->first_event[0] ? r->first_event : "(none)",
            r->last_event[0]  ? r->last_event  : "(none)");

    /* ---- Server Lifecycle ---- */
    print_header(out, "1. SERVER LIFECYCLE", W);
    fprintf(out, "\n");
    fprintf(out, "  %-30s %d\n", "Server Starts:", r->server_starts);
    fprintf(out, "  %-30s %d\n", "Server Stops:", r->server_stops);

    /* ---- Connection Statistics ---- */
    print_header(out, "2. CONNECTION STATISTICS", W);
    fprintf(out, "\n");
    fprintf(out, "  %-30s %d\n", "Total Connections:", r->total_connections);
    fprintf(out, "  %-30s %d\n", "Total Disconnections:", r->total_disconnections);
    fprintf(out, "  %-30s %d\n", "Successful Logins:", r->auth_success);
    fprintf(out, "  %-30s %d\n", "Failed Logins:", r->auth_fail);
    if (r->total_connections > 0)
        fprintf(out, "  %-30s %.1f%%\n", "Auth Success Rate:",
                100.0 * r->auth_success / r->total_connections);

    /* ---- Message Statistics ---- */
    print_header(out, "3. MESSAGE STATISTICS", W);
    fprintf(out, "\n");
    fprintf(out, "  %-30s %ld\n", "Total Messages:", r->total_messages);
    fprintf(out, "  %-30s %ld\n", "Broadcast Messages:", r->broadcast_messages);
    fprintf(out, "  %-30s %ld\n", "Private Messages:", r->private_messages);
    if (r->user_count > 0)
        fprintf(out, "  %-30s %.1f\n", "Avg Messages/User:",
                (double)r->total_messages / r->user_count);

    if (r->total_messages > 0) {
        fprintf(out, "\n  Message Distribution:\n");
        fprintf(out, "  %-20s %-10s %-10s\n", "Type", "Count", "Percentage");
        fprintf(out, "  %-20s %-10s %-10s\n", "----", "-----", "----------");
        fprintf(out, "  %-20s %-10ld %6.1f%%\n", "Broadcast",
                r->broadcast_messages,
                100.0 * r->broadcast_messages / r->total_messages);
        fprintf(out, "  %-20s %-10ld %6.1f%%\n", "Private",
                r->private_messages,
                100.0 * r->private_messages / r->total_messages);
    }

    /* ---- Room Analysis ---- */
    print_header(out, "4. ROOM ANALYSIS", W);
    fprintf(out, "\n");
    fprintf(out, "  %-30s %d\n", "Rooms Created:", r->rooms_created);
    fprintf(out, "  %-30s %d\n", "Unique Rooms Seen:", r->room_count);
    fprintf(out, "\n");

    if (r->room_count > 0) {
        fprintf(out, "  %-16s %-10s %-10s %-10s\n",
                "Room Name", "Messages", "Joins", "Leaves");
        fprintf(out, "  %-16s %-10s %-10s %-10s\n",
                "---------", "--------", "-----", "------");

        for (int i = 0; i < r->room_count; i++) {
            fprintf(out, "  %-16s %-10d %-10d %-10d\n",
                    r->rooms[i].name,
                    r->rooms[i].total_messages,
                    r->rooms[i].joins,
                    r->rooms[i].leaves);
        }
    } else {
        fprintf(out, "  (No room activity recorded)\n");
    }

    /* ---- User Activity ---- */
    print_header(out, "5. USER ACTIVITY", W);
    fprintf(out, "\n");

    if (r->user_count > 0) {
        fprintf(out, "  %-16s %-8s %-6s %-6s %-6s %-20s\n",
                "Username", "Msgs", "PMs", "Rooms", "Login", "Last Seen");
        fprintf(out, "  %-16s %-8s %-6s %-6s %-6s %-20s\n",
                "--------", "----", "---", "-----", "-----", "---------");

        for (int i = 0; i < r->user_count; i++) {
            fprintf(out, "  %-16s %-8d %-6d %-6d %-6d %-20s\n",
                    r->users[i].username,
                    r->users[i].messages_sent,
                    r->users[i].private_msgs_sent,
                    r->users[i].rooms_joined,
                    r->users[i].login_count,
                    r->users[i].last_seen[0] ? r->users[i].last_seen : "-");
        }
    } else {
        fprintf(out, "  (No user activity recorded)\n");
    }

    /* ---- Memory Estimates ---- */
    print_header(out, "6. MEMORY USAGE ESTIMATES", W);
    fprintf(out, "\n");

    int peak_clients = r->auth_success;  /* Upper bound */
    size_t mem_per_client = 152;  /* sizeof(client_t) approx */
    size_t mem_queue = 256 * 2048;
    size_t mem_rooms = r->room_count * 40;

    fprintf(out, "  %-30s %d\n", "Peak Authenticated Users:", peak_clients);
    fprintf(out, "  %-30s %zu bytes/client\n", "Memory per Client:", mem_per_client);
    fprintf(out, "  %-30s %zu bytes (%.2f KB)\n",
            "Est. Client Memory (peak):",
            mem_per_client * peak_clients,
            (mem_per_client * peak_clients) / 1024.0);
    fprintf(out, "  %-30s %zu bytes (%.2f KB)\n",
            "Message Queue:",
            mem_queue, mem_queue / 1024.0);
    fprintf(out, "  %-30s %zu bytes\n", "Room Structures:", mem_rooms);

    fprintf(out, "\n  Dynamic Memory Management:\n");
    fprintf(out, "  - Client structs allocated with calloc() on connect\n");
    fprintf(out, "  - Client structs freed with free() on disconnect\n");
    fprintf(out, "  - Message queue uses fixed-size circular buffer\n");
    fprintf(out, "  - Room data uses static array with active flags\n");

    /* ---- Summary ---- */
    print_header(out, "7. SUMMARY", W);
    fprintf(out, "\n");

    if (r->total_lines == 0) {
        fprintf(out, "  No events found in log file.\n");
        fprintf(out, "  Make sure the server has been run at least once\n");
        fprintf(out, "  and that server.log exists in the working directory.\n");
    } else {
        fprintf(out, "  Over the reporting period, the server handled\n");
        fprintf(out, "  %d connections from %d unique users who exchanged\n",
                r->total_connections, r->user_count);
        fprintf(out, "  %ld messages across %d rooms.\n",
                r->total_messages, r->room_count);

        if (r->auth_fail > 0)
            fprintf(out, "  %d authentication failures were recorded.\n",
                    r->auth_fail);

        fprintf(out, "\n");
    }

    print_separator(out, '=', W);
    fprintf(out, "  End of Report\n");
    print_separator(out, '=', W);
}

/* ========================== Main ========================================= */

int main(int argc, char *argv[])
{
    const char *log_file    = "server.log";
    const char *output_file = NULL;
    FILE *out = stdout;

    if (argc > 1)
        log_file = argv[1];
    if (argc > 2)
        output_file = argv[2];

    printf("\n");
    printf("╔═══════════════════════════════════════════════════╗\n");
    printf("║    Chat Application - Report Generator            ║\n");
    printf("║    Iqra University - OS Lab                       ║\n");
    printf("╚═══════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("Reading log file: %s\n", log_file);

    /* Parse the log */
    report_t report;
    memset(&report, 0, sizeof(report_t));

    int lines = parse_log_file(log_file, &report);
    if (lines < 0) {
        fprintf(stderr,
                "\nNo log file found. Run the server first to generate server.log.\n"
                "Usage: %s [log_file] [output_file]\n\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    printf("Parsed %d log entries.\n", lines);
    printf("Found %d users, %d rooms, %ld messages.\n",
           report.user_count, report.room_count, report.total_messages);

    /* Open output file if specified */
    if (output_file) {
        out = fopen(output_file, "w");
        if (!out) {
            fprintf(stderr, "Error: Cannot open output file: %s\n", output_file);
            return EXIT_FAILURE;
        }
        printf("Writing report to: %s\n", output_file);
    } else {
        printf("\n--- Report ---\n");
    }

    generate_report(out, &report);

    if (output_file) {
        fclose(out);
        printf("\n✓ Report saved to: %s\n", output_file);
    }

    printf("\nDone.\n");
    return EXIT_SUCCESS;
}
