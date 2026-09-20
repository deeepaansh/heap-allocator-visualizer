#ifndef VISUALIZER_H
#define VISUALIZER_H

#include <stddef.h>
#include "include/heap.h"

#define HEAP_SIZE 1024

/* ── ANSI macros ─────────────────────────────────────────────────── */
#define RED   "\033[41m"   /* bg red   — used blocks  */
#define GREEN "\033[42m"   /* bg green — free blocks  */
#define CYAN  "\033[46m"   /* bg cyan  — freed rows in table */
#define RESET "\033[0m"
#define BOLD  "\033[1m"
#define DIM   "\033[2m"

/* Fixed terminal columns consumed by each heap-map block */
#define BLOCK_WIDTH  10

/* Maximum simultaneous tracked allocations */
#define MAX_ALLOCS   64

/* ── Allocation record ───────────────────────────────────────────── */
typedef struct {
    int    id;
    void  *ptr;     /* user pointer as returned by heap_alloc */
    size_t size;    /* requested bytes                         */
    int    active;  /* 1 = live, 0 = freed                    */
} Allocation;

/*
 * heap_dump   — three-line coloured heap map
 *               line 1: [id]USED / [ ]FREE labels (BLOCK_WIDTH cols each)
 *               line 2: data sizes
 *               line 3: byte offsets from heap start
 */
void heap_dump(heap_t *heap, Allocation *table, int count);

/* print_alloc_table — tabular list of every allocation ever made */
void print_alloc_table(Allocation *table, int count);

/* print_stats — single stats line: used / free / fragment count */
void print_stats(heap_t *heap);

/* count_fragments — returns number of free blocks in [heap_start, heap_start + HEAP_SIZE) */
int count_fragments(char *heap_start, char *heap_end);

#endif
