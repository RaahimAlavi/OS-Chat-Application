/*
 * ============================================================================
 * OS-Chat-Application - Authentication Module
 * ============================================================================
 * Department of Software Engineering, Iqra University
 * Operating System Lab - Dynamic Data Structures
 *
 * Provides file-based user credential storage with simple hash-based password
 * protection.  Supports user registration and login verification.
 * ============================================================================
 */

#ifndef AUTH_H
#define AUTH_H

#include "common.h"

#define AUTH_FILE       "users.dat"
#define MAX_USERS_DB    256

/* ----------------------------- Credential Record ------------------------- */
typedef struct {
    char username[USERNAME_LEN];
    char password_hash[PASSWORD_LEN];   /* Simple DJB2 hash stored as hex    */
} credential_t;

/* ----------------------------- Hash Function ----------------------------- */

/**
 * djb2_hash - Dan Bernstein's DJB2 string hash.
 * @str:  Null-terminated input string.
 * Returns: 64-bit hash value.
 *
 * This is NOT cryptographically secure — it is used for demonstration
 * purposes in this lab exercise.
 */
static unsigned long djb2_hash(const char *str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;   /* hash * 33 + c */
    return hash;
}

/* ----------------------------- Auth Functions ---------------------------- */

/**
 * auth_register - Registers a new user.
 * @username:  Desired username (must be unique).
 * @password:  Plain-text password (will be hashed before storage).
 * Returns:    1 on success, 0 if username already exists, -1 on I/O error.
 */
static int auth_register(const char *username, const char *password)
{
    FILE *fp;
    credential_t cred;

    /* Check for existing user */
    fp = fopen(AUTH_FILE, "rb");
    if (fp) {
        while (fread(&cred, sizeof(credential_t), 1, fp) == 1) {
            if (strcmp(cred.username, username) == 0) {
                fclose(fp);
                return 0;   /* Username already taken */
            }
        }
        fclose(fp);
    }

    /* Append new credential */
    fp = fopen(AUTH_FILE, "ab");
    if (!fp)
        return -1;

    memset(&cred, 0, sizeof(credential_t));
    strncpy(cred.username, username, USERNAME_LEN - 1);
    snprintf(cred.password_hash, PASSWORD_LEN, "%lx", djb2_hash(password));

    if (fwrite(&cred, sizeof(credential_t), 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 1;
}

/**
 * auth_login - Verifies user credentials.
 * @username:  The username to verify.
 * @password:  The plain-text password to check.
 * Returns:    1 on success, 0 on invalid credentials, -1 on I/O error.
 */
static int auth_login(const char *username, const char *password)
{
    FILE *fp;
    credential_t cred;
    char hash_str[PASSWORD_LEN];

    fp = fopen(AUTH_FILE, "rb");
    if (!fp)
        return -1;

    snprintf(hash_str, PASSWORD_LEN, "%lx", djb2_hash(password));

    while (fread(&cred, sizeof(credential_t), 1, fp) == 1) {
        if (strcmp(cred.username, username) == 0 &&
            strcmp(cred.password_hash, hash_str) == 0) {
            fclose(fp);
            return 1;   /* Authenticated */
        }
    }

    fclose(fp);
    return 0;   /* Invalid credentials */
}

#endif /* AUTH_H */
