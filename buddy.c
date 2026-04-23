#include "buddy.h"
#include <stddef.h>
#define NULL ((void *)0)

#define MAX_RANK 16
#define PAGE_SIZE 4096

typedef struct free_block {
    struct free_block *next;
} free_block_t;

static void *base_addr = NULL;
static int total_pages = 0;
static free_block_t *free_lists[MAX_RANK + 1]; // 1-indexed
static int allocated_blocks[MAX_RANK + 1]; // Track allocated blocks per rank

// Simple bitmap to track allocation status
#define MAX_TOTAL_PAGES (128 * 1024 / 4)  // 128MB / 4KB
static char page_allocation[MAX_TOTAL_PAGES];  // 0 = free, 1 = allocated

static int get_buddy(void *block, int rank) {
    size_t block_size = PAGE_SIZE * (1 << (rank - 1));
    size_t offset = (char *)block - (char *)base_addr;
    size_t buddy_offset = offset ^ block_size;
    return (buddy_offset < total_pages * PAGE_SIZE) ? buddy_offset : -1;
}

// Static array to track tails of free lists for O(1) append
static free_block_t *free_list_tails[MAX_RANK + 1] = {NULL};

// Track counts for each rank
static int free_counts[MAX_RANK + 1] = {0};

static void add_to_free_list(void *block, int rank) {
    free_block_t *fb = (free_block_t *)block;
    fb->next = NULL;

    if (!free_lists[rank]) {
        free_lists[rank] = fb;
        free_list_tails[rank] = fb;
    } else {
        free_list_tails[rank]->next = fb;
        free_list_tails[rank] = fb;
    }
    free_counts[rank]++;
}

static void *remove_from_free_list(int rank) {
    if (!free_lists[rank]) return NULL;
    free_block_t *fb = free_lists[rank];
    free_lists[rank] = fb->next;

    // Update tail if necessary
    if (free_lists[rank] == NULL) {
        free_list_tails[rank] = NULL;
    }

    free_counts[rank]--;
    return fb;
}

int init_page(void *p, int pgcount) {
    if (!p || pgcount <= 0 || pgcount > MAX_TOTAL_PAGES) return -EINVAL;

    base_addr = p;
    total_pages = pgcount;

    // Initialize free lists
    for (int i = 1; i <= MAX_RANK; i++) {
        free_lists[i] = NULL;
        allocated_blocks[i] = 0;
    }

    // Clear allocation bitmap
    for (int i = 0; i < pgcount; i++) {
        page_allocation[i] = 0;
    }

    // Add all memory as individual pages (rank 1) initially
    // Process in reverse order to avoid traversing the list for each insertion
    if (pgcount > 0) {
        void *last_block = (char *)p + (pgcount - 1) * PAGE_SIZE;
        add_to_free_list(last_block, 1);

        for (int i = pgcount - 2; i >= 0; i--) {
            void *block = (char *)p + i * PAGE_SIZE;
            free_block_t *fb = (free_block_t *)block;
            fb->next = free_lists[1];
            free_lists[1] = fb;
        }
    }

    return OK;
}

void *alloc_pages(int rank) {
    if (rank < 1 || rank > MAX_RANK) return ERR_PTR(-EINVAL);

    // Find the smallest rank >= requested rank that has free blocks
    int alloc_rank = rank;
    while (alloc_rank <= MAX_RANK && !free_lists[alloc_rank]) {
        alloc_rank++;
    }

    if (alloc_rank > MAX_RANK) return ERR_PTR(-ENOSPC);

    // Split larger blocks if necessary
    while (alloc_rank > rank) {
        void *block = remove_from_free_list(alloc_rank);
        if (!block) return ERR_PTR(-ENOSPC);

        // Split the block into two buddies
        int half_size = PAGE_SIZE * (1 << (alloc_rank - 2));
        void *buddy = (char *)block + half_size;

        add_to_free_list(block, alloc_rank - 1);
        add_to_free_list(buddy, alloc_rank - 1);

        alloc_rank--;
    }

    // Now we should have a block of the exact rank needed
    void *result = remove_from_free_list(rank);
    if (result) {
        allocated_blocks[rank]++;
        // Mark pages as allocated
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

    // Check if pointer is within bounds
    size_t offset = (char *)p - (char *)base_addr;
    if (offset >= total_pages * PAGE_SIZE || offset % PAGE_SIZE != 0) {
        return -EINVAL;
    }

    // Check if page is already free
    int page_idx = offset / PAGE_SIZE;
    if (!page_allocation[page_idx]) {
        return -EINVAL;  // Page is already free
    }

    // Calculate the rank of this block
    int rank = query_ranks(p);
    if (rank < 1 || rank > MAX_RANK) return -EINVAL;

    // Mark pages as free
    size_t offset = (char *)p - (char *)base_addr;
    int page_idx = offset / PAGE_SIZE;
    int pages_in_block = 1 << (rank - 1);
    for (int i = 0; i < pages_in_block; i++) {
        page_allocation[page_idx + i] = 0;
    }

    // Try to merge with buddy
    void *current = p;
    int current_rank = rank;

    // Add the block to its free list first
    add_to_free_list(current, current_rank);

    // Try to merge with buddy
    while (current_rank < MAX_RANK) {
        size_t block_size = PAGE_SIZE * (1 << (current_rank - 1));
        size_t offset = (char *)current - (char *)base_addr;
        size_t buddy_offset = offset ^ block_size;

        // Find buddy in free list
        free_block_t **prev = &free_lists[current_rank];
        free_block_t *curr = free_lists[current_rank];
        free_block_t *buddy = NULL;

        // Skip our own block
        if (curr == (free_block_t *)current) {
            prev = &curr->next;
            curr = curr->next;
        }

        while (curr) {
            if ((char *)curr == (char *)base_addr + buddy_offset) {
                buddy = curr;
                break;
            }
            prev = &curr->next;
            curr = curr->next;
        }

        if (!buddy) break;

        // Remove buddy from free list
        *prev = buddy->next;

        // Update tail if we removed the last element
        if (free_lists[current_rank] == NULL) {
            free_list_tails[current_rank] = NULL;
        } else if (buddy == free_list_tails[current_rank]) {
            // Find new tail
            free_block_t *new_tail = free_lists[current_rank];
            while (new_tail->next) {
                new_tail = new_tail->next;
            }
            free_list_tails[current_rank] = new_tail;
        }

        // Remove current block from free list
        if (free_lists[current_rank] == (free_block_t *)current) {
            free_lists[current_rank] = ((free_block_t *)current)->next;
        }

        // Merge and add to higher rank
        if (offset < buddy_offset) {
            add_to_free_list(current, current_rank + 1);
        } else {
            add_to_free_list((char *)base_addr + buddy_offset, current_rank + 1);
            current = (char *)base_addr + buddy_offset;
        }

        current_rank++;
    }

    allocated_blocks[rank]--;

    // Track free count changes during merging
    // We added the original block back
    int final_rank = current_rank;

    // If we merged, we need to adjust counts
    if (final_rank > rank) {
        // We removed from lower ranks and added to higher rank
        for (int r = rank; r < final_rank; r++) {
            free_counts[r]--;
        }
        free_counts[final_rank]++;
    }

    return OK;
}

int query_ranks(void *p) {
    if (!p || !base_addr) return -EINVAL;

    size_t offset = (char *)p - (char *)base_addr;
    if (offset >= total_pages * PAGE_SIZE || offset % PAGE_SIZE != 0) {
        return -EINVAL;
    }

    int page_idx = offset / PAGE_SIZE;

    // Quick check for allocated pages - they are rank 1 in our implementation
    if (page_allocation[page_idx]) {
        return 1;
    }

    // For free pages, find the maximum contiguous free block
    // Start with rank 1 and work up
    int max_rank = 1;

    // Check alignment for larger blocks
    for (int rank = 2; rank <= MAX_RANK; rank++) {
        size_t block_size = PAGE_SIZE * (1 << (rank - 1));

        // Check if offset is aligned for this rank
        if (offset % block_size != 0) break;

        int pages_in_block = 1 << (rank - 1);
        int start_page = page_idx;

        // Check if all pages in this block are free
        if (start_page + pages_in_block > total_pages) break;

        int all_free = 1;
        // Only check the first and last page for efficiency
        if (page_allocation[start_page] || page_allocation[start_page + pages_in_block - 1]) {
            all_free = 0;
        }

        if (!all_free) break;

        max_rank = rank;
    }

    return max_rank;
}

int query_page_counts(int rank) {
    if (rank < 1 || rank > MAX_RANK) return -EINVAL;
    return free_counts[rank];
}
