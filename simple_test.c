#include <stdio.h>
#include <stdlib.h>
#include "buddy.h"

int main() {
    printf("Simple test starting...\n");

    // Allocate 128MB
    void *p = malloc(128 * 1024 * 1024);
    if (!p) {
        printf("Failed to allocate memory\n");
        return 1;
    }

    printf("Memory allocated at %p\n", p);

    // Initialize pages
    int ret = init_page(p, 128 * 1024 / 4);
    printf("init_page returned %d\n", ret);

    // Test alloc
    void *r1 = alloc_pages(1);
    printf("First alloc returned %p\n", r1);

    void *r2 = alloc_pages(1);
    printf("Second alloc returned %p\n", r2);

    // Test query_ranks
    int rank = query_ranks(r1);
    printf("Rank of first allocation: %d\n", rank);

    // Test return
    ret = return_pages(r1);
    printf("return_pages returned %d\n", ret);

    // Test query_page_counts
    int count = query_page_counts(1);
    printf("Free pages of rank 1: %d\n", count);

    printf("Test completed\n");
    return 0;
}