#include <stdio.h>
#include <stdlib.h>
#include "buddy.h"

int main() {
    printf("Starting debug...\n");
    fflush(stdout);

    void *p = malloc(128 * 1024 * 1024);
    printf("Memory allocated\n");
    fflush(stdout);

    int ret = init_page(p, 128 * 1024 / 4);
    printf("init_page returned %d\n", ret);
    fflush(stdout);

    // Test just the first few allocations
    for (int i = 0; i < 10; i++) {
        printf("Allocating page %d\n", i);
        fflush(stdout);
        void *r = alloc_pages(1);
        printf("Got %p\n", r);
        fflush(stdout);
    }

    printf("Test completed\n");
    return 0;
}