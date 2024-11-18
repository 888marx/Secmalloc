#ifndef _SECMALLOC_PRIVATE_H
#define _SECMALLOC_PRIVATE_H

#include "my_secmalloc.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h> // Includes the library to use getenv
#include <unistd.h>
#include <sys/mman.h>
#include <stdarg.h> // Includes functions to manage variable arguments
#include <pthread.h>  // Pour utiliser les mutex
#include <fcntl.h>

// ==========================================
// Structure definitions
// ==========================================

// Data structure for each allocated zone
struct data_pool {
    size_t size;                   // Size of the allocated zone
    struct data_pool *next;        // Pointer to the next data_pool
    struct data_pool *prev;        // Pointer to the previous data_pool
};

// Metadata structure for each allocated zone
struct metadata_pool {
    struct data_pool *data_pool;   // Pointer to the associated data pool
    void *ptr;                     // Pointer to the start of the allocated zone
    struct metadata_pool *next;    // Pointer to the next metadata
    struct metadata_pool *prev;    // Pointer to the previous metadata
    size_t size;                   // Size of the allocated zone
    int is_allocated;              // 1 if the zone is free, 0 otherwise
};

// ==========================================
// Function declarations
// ==========================================

// Function declarations
struct data_pool *init_data_pool(size_t size);        // Initializes a data_pool structure
struct metadata_pool *init_metadata_pool(void *ptr, size_t size, int is_allocated); // Initializes a metadata_pool structure

// Global variables
#define CANARY_VALUE 0xdeadbeef // Define a fixed canary value
extern struct metadata_pool *head; // Declare the variable

void write_trace(const char *format, ...);
void open_report_file();
void detect_memory_leak();
void detect_double_free(void *ptr);
void *my_malloc(size_t size);
void my_free(void *ptr);
void *my_calloc(size_t nmemb, size_t size);
void *my_realloc(void *ptr, size_t size);


#endif