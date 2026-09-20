#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "visualizer.h"

/*
 * Memory layout produced by CCareaga's allocator:
 *
 *   [ node_t (24B) | <data: node->size bytes> | footer_t (8B) ]
 *
 *   node_t.hole == 0  →  allocated/used block
 *   node_t.hole == 1  →  free block
 *
 *   heap_alloc() returns &found->next, i.e. (char*)node + offsetof(node_t,next)
 *   heap_free()  backs up by the same offset to recover the node pointer
 *
 * Walking all blocks (free + used):
 *   curr += sizeof(node_t) + curr->size + sizeof(footer_t)
 */

#define MAX_BLOCKS 256

/* Per-block metadata collected during a heap walk */
typedef struct {
    int    is_used;
    uint   data_size;
    int    alloc_id;   /* ≥0 = known ID, -1 = unknown / free   */
    size_t offset;     /* byte offset from heap->start           */
} BlockInfo;

/* ─── internal: walk the heap and fill out[] ─────────────────────── */
static int collect_blocks(heap_t *heap,
                           Allocation *table, int tcount,
                           BlockInfo *out, int max)
{
    int     n        = 0;
    size_t  ptr_off  = offsetof(node_t, next); /* user ptr = node + ptr_off */
    node_t *curr     = (node_t *)(uintptr_t)heap->start;
    node_t *end      = (node_t *)((char *)(uintptr_t)heap->start + HEAP_SIZE);

    while (curr < end && n < max) {
        out[n].is_used   = (curr->hole == 0);
        out[n].data_size = curr->size;
        out[n].offset    = (size_t)((char *)curr - (char *)(uintptr_t)heap->start);
        out[n].alloc_id  = -1;

        if (out[n].is_used) {
            /* Match against live table entries to find the allocation ID */
            void *user_ptr = (char *)curr + ptr_off;
            for (int i = 0; i < tcount; i++) {
                if (table[i].active && table[i].ptr == user_ptr) {
                    out[n].alloc_id = table[i].id;
                    break;
                }
            }
        }

        n++;
        curr = (node_t *)((char *)curr
               + sizeof(node_t) + curr->size + sizeof(footer_t));
    }
    return n;
}

/* ─── heap_dump: three-line coloured map ────────────────────────── */
void heap_dump(heap_t *heap, Allocation *table, int count)
{
    BlockInfo blocks[MAX_BLOCKS];
    int n = collect_blocks(heap, table, count, blocks, MAX_BLOCKS);
    int W = BLOCK_WIDTH;

    if (n == 0) {
        printf("  (empty)\n");
        return;
    }

    /* Line 1 — coloured label row ─────────────────────────────────
     * Each block occupies exactly W visible terminal columns.
     * ANSI escape sequences are injected around the W-char content
     * so they don't disturb column counting.                        */
    for (int i = 0; i < n; i++) {
        char label[24];
        if (blocks[i].is_used) {
            if (blocks[i].alloc_id >= 0)
                snprintf(label, sizeof(label), "[%d]USED", blocks[i].alloc_id);
            else
                snprintf(label, sizeof(label), "[?]USED");
        } else {
            snprintf(label, sizeof(label), "[ ]FREE");
        }
        /* %-W.Ws: left-align, pad to W, truncate at W visible chars */
        printf("%s%-*.*s%s",
               blocks[i].is_used ? RED : GREEN,
               W, W, label,
               RESET);
    }
    printf("\n");

    /* Line 2 — size row ───────────────────────────────────────────*/
    for (int i = 0; i < n; i++) {
        char sz[24];
        snprintf(sz, sizeof(sz), "%uB", blocks[i].data_size);
        printf("%-*.*s", W, W, sz);
    }
    printf("\n");

    /* Line 3 — address (offset) row ──────────────────────────────*/
    for (int i = 0; i < n; i++) {
        char addr[24];
        snprintf(addr, sizeof(addr), "0x%04zX", blocks[i].offset);
        printf("%-*.*s", W, W, addr);
    }
    printf("\n");
}

/* ─── print_alloc_table ─────────────────────────────────────────── */
void print_alloc_table(Allocation *table, int count)
{
    printf("  %-4s  %-8s  %-8s  %s\n",
           "ID", "Size", "Status", "Pointer");
    printf("  %-4s  %-8s  %-8s  %s\n",
           "----", "--------", "--------", "-------------------");

    if (count == 0) {
        printf("  (no allocations yet)\n");
        return;
    }

    for (int i = 0; i < count; i++) {
        const char *color  = table[i].active ? RED   : GREEN;
        const char *status = table[i].active ? "ACTIVE" : "FREED";
        char sz[16];
        snprintf(sz, sizeof(sz), "%zuB", table[i].size);
        printf("  %-4d  %-8s  %s%-8s%s  %p\n",
               table[i].id,
               sz,
               color, status, RESET,
               table[i].ptr);
    }
}

/* ─── print_stats ───────────────────────────────────────────────── */
void print_stats(heap_t *heap)
{
    node_t      *curr  = (node_t *)(uintptr_t)heap->start;
    node_t      *end   = (node_t *)((char *)(uintptr_t)heap->start + HEAP_SIZE);
    unsigned int used  = 0, free_b = 0, frags = 0;

    while (curr < end) {
        if (curr->hole == 0)
            used   += curr->size;
        else {
            free_b += curr->size;
            frags++;
        }
        curr = (node_t *)((char *)curr
               + sizeof(node_t) + curr->size + sizeof(footer_t));
    }

    printf("  Used: %uB | Free: %uB | Free Fragments: %u\n",
           used, free_b, frags);
}

/* ─── count_fragments ───────────────────────────────────────────── */
int count_fragments(char *heap_start, char *heap_end)
{
    node_t *curr  = (node_t *)(uintptr_t)heap_start;
    node_t *end   = (node_t *)(heap_start + HEAP_SIZE);
    int     count = 0;

    while (curr < end) {
        if (curr->hole == 1) {
            count++;
        }
        curr = (node_t *)((char *)curr
               + sizeof(node_t) + curr->size + sizeof(footer_t));
    }
    return count;
}
