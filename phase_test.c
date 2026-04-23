#include <stdio.h>
#include <stdlib.h>
#include "buddy.h"

#define MAXRANK0PAGE (128 * 1024 / 4)

int main() {
    printf("Phase 1: init\n");
    void *p = malloc(128 * 1024 * 1024);
    int ret = init_page(p, MAXRANK0PAGE);
    printf("init_page returned %d\n", ret);

    printf("\nPhase 2: alloc all pages\n");
    void *q = p;
    for (int pgIdx = 0; pgIdx < MAXRANK0PAGE; pgIdx++) {
        void *r = alloc_pages(1);
        if (r != q) {
            printf("ERROR at index %d: expected %p, got %p\n", pgIdx, q, r);
            break;
        }
        if (pgIdx % 1000 == 0) printf("Allocated %d pages\n", pgIdx);
        q = q + 1024 * 4;
    }

    printf("\nPhase 3: check no more space\n");
    void *r = alloc_pages(1);
    printf("alloc_pages after full: %p (error: %ld)\n", r, PTR_ERR(r));

    printf("\nPhase 4: return all pages\n");
    q = p;
    for (int pgIdx = 0; pgIdx < MAXRANK0PAGE; pgIdx++) {
        ret = return_pages(q);
        if (ret != 0) {
            printf("ERROR returning page %d: %d\n", pgIdx, ret);
            break;
        }
        if (pgIdx % 1000 == 0) printf("Returned %d pages\n", pgIdx);
        q = q + 1024 * 4;
    }

    printf("\nTest completed successfully!\n");
    return 0;
}