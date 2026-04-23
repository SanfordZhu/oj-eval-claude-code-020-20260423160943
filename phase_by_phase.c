#include <stdio.h>
#include <stdlib.h>
#include "buddy.h"

#define MAXRANK (16)
#define TESTSIZE (128)
#define MAXRANK0PAGE (TESTSIZE * 1024 / 4)

int main() {
    printf("Phase 1: init\n");
    void *p = malloc(TESTSIZE * 1024 * 1024);
    int ret = init_page(p, MAXRANK0PAGE);
    printf("init_page returned %d\n", ret);

    printf("\nPhase 2: alloc all pages (first 10 only for testing)\n");
    void *q = p;
    for (int pgIdx = 0; pgIdx < 10; pgIdx++, q = q + 1024 * 4) {
        void *r = alloc_pages(1);
        printf("Allocated page %d: got %p, expected %p\n", pgIdx, r, q);
        if (r != q) {
            printf("ERROR: Mismatch!\n");
            break;
        }
    }

    printf("\nPhase 3: query_page_counts\n");
    for (int pgIdx = 1; pgIdx <= MAXRANK; ++pgIdx) {
        int count = query_page_counts(pgIdx);
        printf("Rank %d: %d free pages\n", pgIdx, count);
    }

    printf("\nPhase 4: return first 5 pages\n");
    q = p;
    for (int pgIdx = 0; pgIdx < 5; pgIdx++, q = q + 1024 * 4) {
        ret = return_pages(q);
        printf("Returned page %d: %d\n", pgIdx, ret);
    }

    printf("\nPhase 5: query ranks of returned pages\n");
    q = p;
    for (int pgIdx = 0; pgIdx < 5; pgIdx++, q = q + 1024 * 4) {
        int rank = query_ranks(q);
        printf("Page %d rank: %d\n", pgIdx, rank);
    }

    printf("\nTest completed\n");
    return 0;
}