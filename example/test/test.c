#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <criterion/criterion.h> // Include Criterion library for unit tests
#include <my_secmalloc.private.h> // Include the header file containing the functions to test
#include <sys/stat.h> // Include the library for file management functions

// ==========================================
// Tests for my_malloc function
// ==========================================

Test(my_malloc, allocates_memory_correctly) {
    size_t size = 100; // Size of the memory to allocate

    // Call the function to test
    void *ptr = my_malloc(size);

    // Assertion to check if the allocation was successful (not NULL)
    cr_assert_not_null(ptr, "(Error) my_malloc() returned NULL");
    my_free(ptr); // Free the allocated memory after the test
}

Test(my_malloc, allocates_large_memory_correctly) {
    size_t size = 1024 * 1024 * 1024; // Size of the memory to allocate (1GB)

    // Call the function to test
    void *ptr = my_malloc(size);

    // Assertion to check if the allocation was successful (not NULL)
    cr_assert_not_null(ptr, "(Error) my_malloc() returned NULL");

    my_free(ptr); // Free the allocated memory after the test
}

Test(my_malloc, allocates_small_memory_correctly) {
    size_t size = 1; // Size of the memory to allocate (1 byte)

    // Call the function to test
    void *ptr = my_malloc(size);

    // Assertion to check if the allocation was successful (not NULL)
    cr_assert_not_null(ptr, "(Error) my_malloc() returned NULL");

    my_free(ptr); // Free the allocated memory after the test
}

Test(my_malloc, finds_suitable_block) {
    size_t size = 100; // Size of the memory to allocate

    // Allocate memory with my_malloc
    void *ptr1 = my_malloc(size);
    cr_assert_not_null(ptr1, "(Error) my_malloc() returned NULL. Memory allocation failed"); // Check if allocation was successful

    // Free the memory
    my_free(ptr1);

    // Allocate memory again with the same size
    void *ptr2 = my_malloc(size);
    cr_assert_not_null(ptr2, "(Error) my_malloc() returned NULL. Memory allocation failed"); // Check if allocation was successful

    // Check if the same block was reused
    cr_assert_eq(ptr1, ptr2, "(Error) my_malloc() did not reuse the suitable block");

    // Free the allocated memory after the test
    my_free(ptr2);
}

Test(my_malloc, finds_suitable_block_and_enlarges_with_mremap) {
    size_t initial_size = 300; // Initial size of the memory to allocate
    size_t new_size = 200; // New size of the memory to allocate

    // Allocate memory with my_malloc
    void *ptr1 = my_malloc(initial_size);
    cr_assert_not_null(ptr1, "(Error) my_malloc() returned NULL. Memory allocation failed"); // Check if allocation was successful

    // Free the memory
    my_free(ptr1);

    // Allocate memory again with a larger size
    void *ptr2 = my_malloc(new_size);
    cr_assert_not_null(ptr2, "(Error) my_malloc() returned NULL. Memory allocation failed"); // Check if allocation was successful

    // Check if the same block was reused and enlarged
    cr_assert_eq(ptr1, ptr2, "(Error) my_malloc() did not reuse and enlarge the suitable block");

    // Free the allocated memory after the test
    my_free(ptr2);
}

// ==========================================
// Tests for my_free function
// ==========================================

Test(my_free, frees_memory_correctly) {
    size_t size = 100; // Size of the memory to allocate

    // Allocate memory with my_malloc
    void *ptr = my_malloc(size);
    cr_assert_not_null(ptr, "(Error) my_malloc() returned NULL. Memory allocation failed"); // Check if allocation was successful

    // Free the memory
    my_free(ptr);

    // Check if the memory was freed correctly
    // Since there's no direct way to check if memory was freed,
    // we can try to allocate memory again and check if it's successful
    void *new_ptr = my_malloc(size);
    cr_assert_not_null(new_ptr, "(Error) my_malloc() returned NULL after freeing memory. Memory might not have been freed correctly");

    // Call the memory leak detection function
    detect_memory_leak();

    // Free the newly allocated memory
    my_free(new_ptr);
}

// ==========================================
// Tests for my_calloc function
// ==========================================

Test(my_calloc, allocates_memory_and_initializes_to_zero) {
    size_t nmemb = 10; // Number of elements to allocate
    size_t size = 100; // Size of each element to allocate

    // Call the function to test
    char *ptr = (char *)my_calloc(nmemb, size);

    // Assertion to check if the allocation was successful (not NULL)
    cr_assert_not_null(ptr, "(Error) my_calloc() returned NULL. Allocation failed");

    // Check if the memory is initialized to zero
    for (size_t i = 0; i < nmemb * size; ++i) {
        cr_assert_eq(ptr[i], 0, "(Error) Memory is not initialized to zero");
    }

    // Call the memory leak detection function
    detect_memory_leak();

    my_free(ptr); // Free the allocated memory after the test
}

// ==========================================
// Tests for my_realloc function
// ==========================================

Test(my_realloc, reallocates_memory_correctly) {
    size_t initial_size = 100; // Initial size of the memory to allocate
    size_t new_size = 200; // New size of the memory to allocate

    // Allocate initial memory with my_malloc
    void *ptr = my_malloc(initial_size);
    cr_assert_not_null(ptr, "(Error) my_malloc() returned NULL. Allocation failed"); // Check if allocation was successful

    // Call the function to test
    void *new_ptr = my_realloc(ptr, new_size);

    // Assertion to check if the reallocation was successful (not NULL)
    cr_assert_not_null(new_ptr, "(Error) my_realloc() returned NULL. Reallocation failed");

    // Call the memory leak detection function
    detect_memory_leak();

    // Free the reallocated memory after the test
    my_free(new_ptr);
}

// Test to check if my_realloc returns NULL when ptr is NULL
Test(my_realloc, returns_null_when_ptr_is_null) {
    size_t size = 100; // Size of the memory to allocate

    // Call the function to test with ptr = NULL
    void *ptr = my_realloc(NULL, size);

    // Assertion to check if the allocation was unsuccessful (NULL)
    cr_assert_null(ptr, "(Error) my_realloc() did not return NULL when ptr is NULL");
}

// Test to check if my_realloc correctly copies the old data to the new location
Test(my_realloc, copies_old_data_to_new_location) {
    size_t initial_size = 100; // Initial size of the memory to allocate
    size_t new_size = 200; // New size of the memory to allocate

    // Allocate memory with my_malloc
    char *initial_ptr = (char *)my_malloc(initial_size);
    cr_assert_not_null(initial_ptr, "(Error) my_malloc() returned NULL. Memory allocation failed"); // Check if allocation was successful

    // Write some data to the allocated memory
    for (size_t i = 0; i < initial_size; ++i) {
        initial_ptr[i] = 'A';
    }

    // Call the function to test
    char *new_ptr = (char *)my_realloc(initial_ptr, new_size);

    // Assertion to check if the reallocation was successful (not NULL)
    cr_assert_not_null(new_ptr, "(Error) my_realloc() returned NULL. Reallocation failed");

    // Check if the old data was correctly copied to the new location
    for (size_t i = 0; i < initial_size; ++i) {
        cr_assert_eq(new_ptr[i], 'A', "(Error) my_realloc() did not correctly copy the old data to the new location");
    }

    // Free the reallocated memory after the test
    my_free(new_ptr);
}