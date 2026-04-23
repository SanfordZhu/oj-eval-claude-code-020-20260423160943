#include "buddy.h"
#include <stddef.h>
#define NULL ((void *)0)

#define MAX_RANK 16
#define PAGE_SIZE 4096
#define MAX_TOTAL_PAGES (128 * 1024 / 4)

typedef struct free_block {
    struct free_block *next;
} free_block_t;

static void *base_addr = NULL;
static int total_pages = 0;
static free_block_t *free_lists[MAX_RANK + 1];
static int allocated_blocks[MAX_RANK + 1];
static char page_allocation[MAX_TOTAL_PAGES];
static int free_counts[MAX_RANK + 1];

static void add_to_free_list(void *block, int rank) {
    if (!block || rank < 1 || rank > MAX_RANK) return;

    free_block_t *fb = (free_block_t *)block;
    fb->next = free_lists[rank];
    free_lists[rank] = fb;
    free_counts[rank]++;
}

static void *remove_from_free_list(int rank) {
    if (rank < 1 || rank > MAX_RANK || !free_lists[rank]) return NULL;

    free_block_t *fb = free_lists[rank];
    free_lists[rank] = fb->next;
    free_counts[rank]--;
    return fb;
}

int init_page(void *p, int pgcount) {
    if (!p || pgcount <= 0 || pgcount > MAX_TOTAL_PAGES) return -EINVAL;

    base_addr = p;
    total_pages = pgcount;

    for (int i = 1; i <= MAX_RANK; i++) {
        free_lists[i] = NULL;
        allocated_blocks[i] = 0;
        free_counts[i] = 0;
    }

    for (int i = 0; i < pgcount; i++) {
        page_allocation[i] = 0;
    }

    // Add all pages in reverse order for efficiency
    for (int i = pgcount - 1; i >= 0; i--) {
        void *block = (char *)p + i * PAGE_SIZE;
        add_to_free_list(block, 1);
    }

    return OK;
}

void *alloc_pages(int rank) {
    if (rank < 1 || rank > MAX_RANK) return ERR_PTR(-EINVAL);

    int alloc_rank = rank;
    while (alloc_rank <= MAX_RANK && !free_lists[alloc_rank]) {
        alloc_rank++;
    }

    if (alloc_rank > MAX_RANK) return ERR_PTR(-ENOSPC);

    while (alloc_rank > rank) {
        void *block = remove_from_free_list(alloc_rank);
        if (!block) return ERR_PTR(-ENOSPC);

        int half_size = PAGE_SIZE * (1 << (alloc_rank - 2));
        void *buddy = (char *)block + half_size;

        add_to_free_list(block, alloc_rank - 1);
        add_to_free_list(buddy, alloc_rank - 1);

        alloc_rank--;
    }

    void *result = remove_from_free_list(rank);
    if (result) {
        allocated_blocks[rank]++;
        size_t offset = (char *)result - (char *)base_addr;
        int page_idx = offset / PAGE_SIZE;
        int pages_in_block = 1 << (rank - 1);
        for (int i = 0; i < pages_in_block; i++) {
            page_allocation[page_idx + i] = 1;
        }
    }

    return result ? result : ERR_PTR(-ENOSPC);
}

int return_pages(void *p) {
    if (!p || !base_addr) return -EINVAL;

    size_t offset = (char *)p - (char *)base_addr;
    if (offset >= total_pages * PAGE_SIZE || offset % PAGE_SIZE != 0) {
        return -EINVAL;
    }

    int page_idx = offset / PAGE_SIZE;
    if (!page_allocation[page_idx]) {
        return -EINVAL;
    }

    int rank = query_ranks(p);
    if (rank < 1 || rank > MAX_RANK) return -EINVAL;

    size_t block_offset = offset;
    int pages_in_block = 1 << (rank - 1);
    for (int i = 0; i < pages_in_block; i++) {
        page_allocation[page_idx + i] = 0;
    }

    allocated_blocks[rank]--;

    // Simple buddy merging
    void *current = p;
    int current_rank = rank;

    while (current_rank < MAX_RANK) {
        size_t block_size = PAGE_SIZE * (1 << (current_rank - 1));
        size_t buddy_offset = block_offset ^ block_size;

        // Find buddy in free list
        free_block_t **prev = &free_lists[current_rank];
        free_block_t *curr = free_lists[current_rank];
        int found = 0;

        while (curr) {
            if ((char *)curr == (char *)base_addr + buddy_offset) {
                *prev = curr->next;
                found = 1;
                break;
            }
            prev = &curr->next;
            curr = curr->next;
        }

        if (!found) break;

        free_counts[current_rank]--;

        if (block_offset < buddy_offset) {
            add_to_free_list(current, current_rank + 1);
        } else {
            add_to_free_list((char *)base_addr + buddy_offset, current_rank + 1);
            current = (char *)base_addr + buddy_offset;
            block_offset = buddy_offset;
        }

        current_rank++;
    }

    add_to_free_list(current, current_rank);

    return OK;
}

int query_ranks(void *p) {
    if (!p || !base_addr) return -EINVAL;

    size_t offset = (char *)p - (char *)base_addr;
    if (offset >= total_pages * PAGE_SIZE || offset % PAGE_SIZE != 0) {
        return -EINVAL;
    }

    int page_idx = offset / PAGE_SIZE;

    if (!page_allocation[page_idx]) {
        int max_rank = 1;
        size_t temp_offset = offset;
        while (temp_offset % (PAGE_SIZE * (1 << max_rank)) == 0 && max_rank < MAX_RANK) {
            int pages_in_block = 1 << max_rank;
            int start_page = temp_offset / PAGE_SIZE;
            if (start_page + pages_in_block > total_pages) break;

            int all_free = 1;
            for (int i = 0; i < pages_in_block; i++) {
                if (page_allocation[start_page + i]) {
                    all_free = 0;
                    break;
                }
            }
            if (!all_free) break;
            max_rank++;
        }
        return max_rank - 1;
    }

    return 1;
}

int query_page_counts(int rank) {
    if (rank < 1 || rank > MAX_RANK) return -EINVAL;
    return free_counts[rank];
}