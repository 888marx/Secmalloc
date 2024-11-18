#define _GNU_SOURCE

// ==========================================
// Includes
// ==========================================

#include "my_secmalloc.private.h"


// ==========================================
// Global variables
// ==========================================

pthread_mutex_t report_mutex = PTHREAD_MUTEX_INITIALIZER;

struct metadata_pool *head = NULL;

// File to record logs during execution
FILE *report_file = NULL;

// ==========================================
// Function definitions
// ==========================================

void misc_log(const char *format, ...) {
    char *output_file = getenv("MSM_OUTPUT");
        va_list args;
        va_start(args, format);
        size_t len = vsnprintf(NULL, 0, format, args);
        va_end(args);

    va_start(args, format) ;
    int fd = open(output_file, O_WRONLY | O_APPEND | O_CREAT, 0644); // Open the file in write mode
    if (fd > 0) {
        char *buffer = alloca(len + 1); // Allocate a buffer on the stack to store the formatted message
        vsnprintf(buffer, len + 1, format, args); // Formatting the message in the buffer
        write(fd, buffer, len); // Write the formatted message to the file
        close(fd); // Close the file
    }
    va_end(args);
}


// Function to detect memory leaks
void detect_memory_leak() {
    struct metadata_pool *current = head; // Start from the top of the list
    while (current != NULL) { // Browse the whole list
        if (current->is_allocated) { // If the block is allocated
            misc_log("Warning ! Memory Leak detected at address: %p, Size: %zu", current->ptr, current->size);
        }
        current = current->next; // Move on to the next block
    }
}

// Function to detect double free
void detect_double_free(void *ptr) {
    struct metadata_pool *current = head;
    while (current != NULL) {
        if (current->ptr == ptr && current->is_allocated == 0) { // If the block is already released
            misc_log("Warning ! Double Free detected at address: %p", ptr);
            return;
        }
        current = current->next;
    }
}


// Function to check for heap overflow
/*void detect_heap_overflow(struct metadata_pool *metadata) {
    // Check if the allocated size is less than the requested size
    if (metadata->allocated_size < metadata->size) {
        write_trace("Heap Overflow detected at address: %p, Requested Size: %zu, Allocated Size: %zu",
                    metadata->ptr, metadata->size, metadata->allocated_size);
    }
}*/

void *my_malloc(size_t size)
{

    misc_log("Start of my_malloc(%zu)\n", size);

    size = (size % 16 ? size + 16 - size % 16 : size); // Align the size to 16 bytes
    // Searching for a suitable block
    struct metadata_pool *metadata = head;
    while (metadata != NULL) {
        if (metadata->is_allocated == 0 && metadata->size >= size) {
            // Suitable block found
            metadata->is_allocated = 1;
            return metadata->ptr;
        }
        metadata = metadata->next;
    }

    // No suitable block found
    size_t total_size = size + sizeof(struct metadata_pool) + sizeof(struct data_pool);

    // Allocate memory using mmap
    void *ptr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (ptr == MAP_FAILED) {
        misc_log("Error : my_malloc(%zu) failed", size);
        return NULL;
    }


    // Set the metadata
    metadata = (struct metadata_pool *)ptr;
    metadata->ptr = ptr + sizeof(struct metadata_pool) + sizeof(struct data_pool);
    metadata->size = size;
    metadata->is_allocated = 1;
    metadata->next = head;

    // Set the data_pool
    struct data_pool *data = (struct data_pool *)(ptr + sizeof(struct metadata_pool));
    data->size = size;
    data->next = NULL;
    data->prev = NULL;

    // Link the metadata and data_pool
    metadata->data_pool = data;

    if (head != NULL)
        head->prev = metadata;
    head = metadata;

    // If the memory area in the data pool doesn't have enough space, enlarge it with mremap
    if (data->size < size) {
        void *new_ptr = mremap(ptr, total_size, total_size + size - data->size, MREMAP_MAYMOVE);
        if (new_ptr == MAP_FAILED) {
            misc_log("Error : mremap fo the data pool failed");
            return NULL;
        }
        ptr = new_ptr;
        data->size = size;
    }

    // If the memory area in the metadata pool doesn't have enough space, enlarge it with mremap
    if (metadata->size < size) {
        void *new_ptr = mremap(ptr, total_size, total_size + size - metadata->size, MREMAP_MAYMOVE);
        if (new_ptr == MAP_FAILED) {
            misc_log("Error : mremap for the metadata failed");
            return NULL;
        }
        ptr = new_ptr;
        metadata->size = size;
    }

    // Placing the canary value at the end of the data block
    unsigned long *canary = (unsigned long *)((char *)metadata->ptr + metadata->data_pool->size);
    *canary = CANARY_VALUE;

    misc_log("End of my_malloc(%zu)\n", size);
    return metadata->ptr;

}

void my_free(void *ptr)
{
    misc_log("Start of my_free(%p)\n", ptr);

    // Detect double free
    detect_double_free(ptr);

    // Find the block to free
    struct metadata_pool *metadata = head;
    while (metadata != NULL) {
        if (metadata->ptr == ptr)
            break;
        metadata = metadata->next;
    }

    // Check if metadata is NULL
    if (metadata == NULL) {
        misc_log("my_free(%p) failed : invalid pointer", ptr);
        return;
    }

    // Check the canary value
    unsigned long *canary = (unsigned long *)((char *)metadata->ptr + metadata->data_pool->size);
    if (*canary != CANARY_VALUE) {
        misc_log("Warning ! Canary has been changed: %p", ptr);
    }

    // Free the block
    metadata->is_allocated = 0;

    // Merge with previous block if it's free
    if (metadata->prev != NULL && !metadata->prev->is_allocated) {
        metadata->prev->size += metadata->size + sizeof(struct metadata_pool) + sizeof(struct data_pool) + sizeof(unsigned long);
        metadata->prev->next = metadata->next;
        if (metadata->next != NULL) {
            metadata->next->prev = metadata->prev;
        }
        metadata = metadata->prev;
    }

    // Merge with next block if it's free
    if (metadata->next != NULL && !metadata->next->is_allocated) {
        metadata->size += metadata->next->size + sizeof(struct metadata_pool) + sizeof(struct data_pool) + sizeof(unsigned long);
        metadata->next = metadata->next->next;
        if (metadata->next != NULL) {
            metadata->next->prev = metadata;
        }
    }

    misc_log("End of my_free(%p)\n", ptr);

    return;
}

void *my_calloc(size_t nmemb, size_t size) {

    misc_log("Start of my_calloc(%zu, %zu)\n", nmemb, size);

    // Calculate the total size of the allocation including the metadata and data_pool
    size_t total_size = nmemb * size + sizeof(struct metadata_pool) + sizeof(struct data_pool);

    // Allocate memory using mmap
    void *ptr = mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (ptr == MAP_FAILED) {
        misc_log("my_calloc(%zu, %zu) failed", nmemb, size);
        return NULL;
    }

    // Set the metadata
    struct metadata_pool *metadata = (struct metadata_pool *)ptr;
    metadata->ptr = ptr + sizeof(struct metadata_pool) + sizeof(struct data_pool);
    metadata->size = nmemb * size;
    metadata->is_allocated = 1;
    metadata->next = head;

    // Set the data_pool
    struct data_pool *data = (struct data_pool *)(ptr + sizeof(struct metadata_pool));
    data->size = nmemb * size;
    data->next = NULL;
    data->prev = NULL;

    // Link the metadata and data_pool
    metadata->data_pool = data;

    if (head != NULL)
        head->prev = metadata;
    head = metadata;

    // Initialize the memory to zero
    memset(metadata->ptr, 0, nmemb * size);

    misc_log("End of my_calloc(%zu, %zu)\n", nmemb, size);
    return metadata->ptr;
}

void *my_realloc(void *ptr, size_t size) {

    misc_log("Start of my_realloc(%p, %zu)\n", ptr, size);

    // Search for the metadata of the block to reallocate
    struct metadata_pool *metadata = head;
    while (metadata != NULL) {
        if (metadata->ptr == ptr)
            break;
        metadata = metadata->next;
    }

    if (metadata == NULL) {
        misc_log("my_realloc(%p, %zu) failed: invalid pointer", ptr, size);
        return NULL; // Pointer not found
    }


    // Allocate new memory
    void *new_ptr = my_malloc(size);
    if (new_ptr == NULL) {
        misc_log("my_realloc(%p, %zu) failed: allocation failed", ptr, size);
        return NULL; // Allocation failed
    }

    // Copy the data from the old memory to the new
    size_t copy_size = size < metadata->size ? size : metadata->size;
    memcpy(new_ptr, ptr, copy_size);

    // Free the old memory
    my_free(ptr);

    // Detect memory leaks
    detect_memory_leak();

    misc_log("End of my_realloc(%p, %zu)\n", ptr, size);
    return new_ptr;
}

#ifdef DYNAMIC
void    *malloc(size_t size)
{
    return my_malloc(size);
}
void    free(void *ptr)
{
    my_free(ptr);
}
void    *calloc(size_t nmemb, size_t size)
{
    return my_calloc(nmemb, size);
}

void    *realloc(void *ptr, size_t size)
{
    return my_realloc(ptr, size);

}

#endif
