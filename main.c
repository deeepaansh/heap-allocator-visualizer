#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "include/heap.h"
#include "include/llist.h"
#include "visualizer.h"

/* ── storage — no stdlib malloc/free anywhere ──────────────────── */
static heap_t     the_heap;
static bin_t      bins[BIN_COUNT];
static char       heap_memory[HEAP_SIZE + sizeof(node_t) + sizeof(footer_t)];
static node_t     dummy_wilderness = { .size = 0x10000, .hole = 1 };

/* ── allocation table ────────────────────────────────────────────── */
Allocation  table[MAX_ALLOCS];
int         alloc_count = 0;
static int  next_id     = 1;

/* ── helper struct for allocation snapshot ───────────────────────── */
typedef struct {
    char    *addr;
    uint32_t size;
} FreeSnapshot;

/* ── status message (shown on next redraw) ───────────────────────── */
static char last_msg[320] = "";

/* ─────────────────────────────────────────────────────────────────── */

static void reset_heap(void)
{
    int i;
    memset(heap_memory, 0, sizeof(heap_memory));
    for (i = 0; i < BIN_COUNT; i++) {
        bins[i].head     = NULL;
        the_heap.bins[i] = &bins[i];
    }

    /* Initialize root chunk of size (HEAP_SIZE - 32) */
    node_t *init_region = (node_t *)heap_memory;
    init_region->hole = 1;
    init_region->size = HEAP_SIZE - sizeof(node_t) - sizeof(footer_t); /* 1024 - 32 = 992 */

    create_foot(init_region);

    uint bin_idx = get_bin_index(init_region->size);
    add_node(the_heap.bins[bin_idx], init_region);

    /* Boundary node right at heap_memory + 1024 so heap_free doesn't coalesce past end */
    node_t *boundary = (node_t *)(heap_memory + HEAP_SIZE);
    boundary->hole = 0;
    boundary->size = 0;

    /* Dummy footer right after boundary node so get_wilderness sees dummy_wilderness */
    footer_t *dummy_foot = (footer_t *)(heap_memory + HEAP_SIZE + sizeof(node_t));
    dummy_foot->header = &dummy_wilderness;

    the_heap.start = (void *)heap_memory;
    the_heap.end   = (void *)(heap_memory + HEAP_SIZE + sizeof(node_t) + sizeof(footer_t));

    memset(table, 0, sizeof(table));
    alloc_count = 0;
    next_id     = 1;
}

/* ─── draw_ui: clear screen + full TUI frame ────────────────────── */
static void draw_ui(void)
{
    /* ANSI: clear screen, move cursor home */
    printf("\033[2J\033[H");

    /* ── Header box ─────────────────────────────────────────────── */
    printf(BOLD
           "+----------------------------------------------------------+\n"
           "|          HEAP ALLOCATOR VISUALIZER                      |\n"
           "|  Heap: %-6d  |  Block overhead: %zuB (hdr + footer)   |\n"
           "+----------------------------------------------------------+\n"
           RESET "\n",
           HEAP_SIZE,
           sizeof(node_t) + sizeof(footer_t));

    /* ── Last-operation message ──────────────────────────────────── */
    if (last_msg[0])
        printf(">> %s\n\n", last_msg);

    /* ── Heap map ────────────────────────────────────────────────── */
    printf(BOLD "  HEAP MAP\n" RESET);
    heap_dump(&the_heap, table, alloc_count);
    printf("\n");

    /* ── Allocation table ────────────────────────────────────────── */
    printf(BOLD "  ALLOCATION TABLE\n" RESET);
    print_alloc_table(table, alloc_count);
    printf("\n");

    /* ── Stats ───────────────────────────────────────────────────── */
    printf(BOLD "  STATS\n" RESET);
    print_stats(&the_heap);
    printf("\n");

    /* ── Commands ────────────────────────────────────────────────── */
    printf(BOLD "  COMMANDS\n" RESET);
    printf("  a <size>   Allocate <size> bytes (assigns next auto ID)\n");
    printf("  f <id>     Free the allocation with that ID\n");
    printf("  r          Reset / reinitialise the heap\n");
    printf("  q          Quit\n");
    printf("\n");
    printf(BOLD "Command> " RESET);
    fflush(stdout);
}

/* ─────────────────────────────────────────────────────────────────── */

int main(void)
{
    reset_heap();
    snprintf(last_msg, sizeof(last_msg),
             "Heap initialised (%dB).  Try: a 32", HEAP_SIZE);

    char line[256];

    for (;;) {
        draw_ui();

        if (!fgets(line, sizeof(line), stdin))
            break;   /* EOF (Ctrl-D) */

        /* strip trailing newline / whitespace */
        line[strcspn(line, "\r\n")] = '\0';

        /* skip leading spaces */
        char *cmd = line;
        while (isspace((unsigned char)*cmd)) cmd++;

        if (!cmd[0]) continue;   /* blank line — just redraw */

        char op = (char)tolower((unsigned char)cmd[0]);

        /* ── q: quit ───────────────────────────────────────────── */
        if (op == 'q') {
            printf("\033[2J\033[H");
            printf("Goodbye!\n");
            break;

        /* ── r: reset ──────────────────────────────────────────── */
        } else if (op == 'r') {
            reset_heap();
            snprintf(last_msg, sizeof(last_msg), "Heap reset.");

        /* ── a <size>: allocate ─────────────────────────────────── */
        } else if (op == 'a') {
            int size = 0;
            if (sscanf(cmd + 1, "%d", &size) != 1 || size <= 0) {
                snprintf(last_msg, sizeof(last_msg),
                         "Usage: a <size>   (size must be a positive integer)");
            } else if (alloc_count >= MAX_ALLOCS) {
                snprintf(last_msg, sizeof(last_msg),
                         "Allocation table full (%d entries max).", MAX_ALLOCS);
            } else {
                /* Snapshot free blocks before alloc */
                FreeSnapshot free_blocks_before[64];
                int free_count_before = 0;

                node_t *curr = (node_t *)(uintptr_t)the_heap.start;
                node_t *end  = (node_t *)(heap_memory + HEAP_SIZE);
                while (curr < end && free_count_before < 64) {
                    if (curr->hole == 1) {
                        free_blocks_before[free_count_before].addr = (char *)curr;
                        free_blocks_before[free_count_before].size = curr->size;
                        free_count_before++;
                    }
                    curr = (node_t *)((char *)curr + sizeof(node_t) + curr->size + sizeof(footer_t));
                }

                void *ptr = heap_alloc(&the_heap, (size_t)size);
                if (!ptr) {
                    snprintf(last_msg, sizeof(last_msg),
                             "heap_alloc(%d) failed — out of memory.", size);
                } else {
                    int id = next_id++;
                    table[alloc_count].id     = id;
                    table[alloc_count].ptr    = ptr;
                    table[alloc_count].size   = (size_t)size;
                    table[alloc_count].active = 1;
                    alloc_count++;

                    /* Find selected free block */
                    char *alloc_addr = (char *)ptr - offsetof(node_t, next);
                    size_t sel_offset = (size_t)(alloc_addr - (char *)(uintptr_t)the_heap.start);

                    uint32_t orig_free_size = 0;
                    for (int i = 0; i < free_count_before; i++) {
                        if (free_blocks_before[i].addr == alloc_addr) {
                            orig_free_size = free_blocks_before[i].size;
                            break;
                        }
                    }

                    int remainder = (int)orig_free_size - size - 32;
                    if (remainder <= 0) {
                        snprintf(last_msg, sizeof(last_msg),
                                 "Allocated %dB as ID %d.\n"
                                 "   Best-fit picked free block at 0x%04zX (exact fit, no split)",
                                 size, id, sel_offset);
                    } else {
                        snprintf(last_msg, sizeof(last_msg),
                                 "Allocated %dB as ID %d.\n"
                                 "   Best-fit picked free block at 0x%04zX (was %uB free → %dB remains after split)",
                                 size, id, sel_offset, orig_free_size, remainder);
                    }
                }
            }

        /* ── f <id>: free ───────────────────────────────────────── */
        } else if (op == 'f') {
            int id = 0;
            if (sscanf(cmd + 1, "%d", &id) != 1) {
                snprintf(last_msg, sizeof(last_msg),
                         "Usage: f <id>");
            } else {
                int found = 0;
                for (int i = 0; i < alloc_count; i++) {
                    if (table[i].id == id && table[i].active) {
                        int fragments_before = count_fragments((char *)the_heap.start, (char *)the_heap.end);
                        size_t freed_size = table[i].size;

                        heap_free(&the_heap, table[i].ptr);
                        table[i].active = 0;

                        int fragments_after = count_fragments((char *)the_heap.start, (char *)the_heap.end);

                        int diff = fragments_after - fragments_before;
                        if (diff == 1) {
                            snprintf(last_msg, sizeof(last_msg),
                                     "Freed ID %d (%zuB). No neighbors to coalesce. Fragments: %d → %d",
                                     id, freed_size, fragments_before, fragments_after);
                        } else if (diff == 0) {
                            snprintf(last_msg, sizeof(last_msg),
                                     "Freed ID %d (%zuB). Coalesced with 1 neighbor. Fragments: %d → %d (unchanged count but block grew)",
                                     id, freed_size, fragments_before, fragments_after);
                        } else if (diff == -1) {
                            snprintf(last_msg, sizeof(last_msg),
                                     "Freed ID %d (%zuB). Coalesced with 1 neighbor. Fragments: %d → %d",
                                     id, freed_size, fragments_before, fragments_after);
                        } else if (diff == -2) {
                            snprintf(last_msg, sizeof(last_msg),
                                     "Freed ID %d (%zuB). Coalesced with BOTH neighbors. Fragments: %d → %d",
                                     id, freed_size, fragments_before, fragments_after);
                        } else {
                            snprintf(last_msg, sizeof(last_msg),
                                     "Freed ID %d (%zuB). Fragments: %d → %d",
                                     id, freed_size, fragments_before, fragments_after);
                        }
                        found = 1;
                        break;
                    }
                }
                if (!found)
                    snprintf(last_msg, sizeof(last_msg),
                             "No active allocation with ID %d.", id);
            }

        /* ── unknown ─────────────────────────────────────────────── */
        } else {
            snprintf(last_msg, sizeof(last_msg),
                     "Unknown command '%c'.  Valid: a <size>, f <id>, r, q", op);
        }
    }

    return 0;
}
