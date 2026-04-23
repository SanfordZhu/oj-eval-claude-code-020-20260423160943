#include <stdio.h>
#include <stdlib.h>
#include "buddy.h"

int main() {
    printf("Safety test starting...\n");
    fflush(stdout);

    // Test 1: NULL parameters
    printf("Test 1: NULL parameters\n");
    fflush(stdout);
    int ret = init_page(NULL, 100);
    printf("init_page(NULL) returned %d\n", ret);
    fflush(stdout);

    ret = return_pages(NULL);
    printf("return_pages(NULL) returned %d\n", ret);
    fflush(stdout);

    int rank = query_ranks(NULL);
    printf("query_ranks(NULL) returned %d\n", rank);
    fflush(stdout);

    // Test 2: Invalid parameters
    printf("\nTest 2: Invalid parameters\n");
    fflush(stdout);
    void *p = malloc(1024 * 1024);
    ret = init_page(p, -1);
    printf("init_page with negative count returned %d\n", ret);
    fflush(stdout);

    ret = init_page(p, 0);
    printf("init_page with zero count returned %d\n", ret);
    fflush(stdout);

    // Test 3: Out of bounds access
    printf("\nTest 3: Out of bounds access\n");
    fflush(stdout);
    ret = init_page(p, 100);
    printf("init_page(100) returned %d\n", ret);
    fflush(stdout);

    rank = query_ranks(p + 1000 * 1024 * 4);  // Way out of bounds
    printf("query_ranks out of bounds returned %d\n", rank);
    fflush(stdout);

    printf("\nSafety test completed\n");
    return 0;
}