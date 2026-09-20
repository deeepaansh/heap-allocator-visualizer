# 🛠️ Heap Allocator Visualizer

An interactive C program that wraps around a dynamic memory allocator and visualizes heap allocation, external fragmentation, best-fit block selection, and neighbor coalescing in the terminal using ANSI colors.

---

## 🌟 Key Features

- **Interactive Full-Screen TUI**: Full-screen terminal dashboard built using ANSI escape sequences.
- **3-Line Coloured Heap Map**:
  - **Line 1 (Status)**: `[ID]USED` (Red) and `[ ]FREE` (Green).
  - **Line 2 (Payload)**: Usable block sizes (e.g., `32B`, `64B`).
  - **Line 3 (Offsets)**: Hex memory offsets from `heap.start` (`0x0000`, `0x0040`).
- **Best-Fit Selection & Remainder Tracking**: Calculates remainder split bytes and alerts on exact-fit allocations (`a <size>`).
- **Coalescing & Fragmentation Engine**: Analyzes fragment count deltas ($\Delta$) when freeing memory (`f <id>`) to detect $O(1)$ neighbor merging.
- **Fixed-Grid Terminal Layout**: 10-column fixed block width prevents large free tails from warping terminal rendering.

---

## 📺 Terminal Interface Preview

```
+----------------------------------------------------------+
|          HEAP ALLOCATOR VISUALIZER                      |
|  Heap: 1024B   |  Block overhead: 32B (hdr + footer)   |
+----------------------------------------------------------+

>> Allocated 16B as ID 4.
   Best-fit picked free block at 0x002A (was 40B free → 16B remains after split)

  HEAP MAP
[4]USED   [ ]FREE   [3]USED   [ ]FREE   
10B       40B       20B       826B      
0x0000    0x002A    0x0072    0x00A6    

  ALLOCATION TABLE
  ID    Size      Status    Pointer
  ----  --------  --------  -------------------
  1     10B       FREED     0x1024c49d0
  2     40B       FREED     0x1024c49fa
  3     20B       ACTIVE    0x1024c4a42
  4     10B       ACTIVE    0x1024c49d0

  STATS
  Used: 30B | Free: 866B | Free Fragments: 2

  COMMANDS
  a <size>   Allocate <size> bytes (assigns next auto ID)
  f <id>     Free the allocation with that ID
  r          Reset / reinitialise the heap
  q          Quit

Command> 
```

---

## 🚀 Quick Start & Local Setup

### Prerequisites
- `clang` or `gcc` compiler
- `make`
- POSIX-compliant terminal with ANSI color support

### Installation & Run

1. **Clone the repository:**
   ```bash
   git clone https://github.com/deeepaansh/heap-allocator-visualizer
   cd heap-allocator-visualizer
   ```

2. **Build the project:**
   ```bash
   make
   ```

3. **Launch the visualizer:**
   ```bash
   ./demo
   ```

4. **Clean build files:**
   ```bash
   make clean
   ```

---

## 🎮 Interactive Commands

| Command | Usage | Description |
|---|---|---|
| **`a <size>`** | `a 32` | Allocates `<size>` payload bytes and assigns an auto-incrementing ID. |
| **`f <id>`** | `f 2` | Frees the allocation associated with ID `<id>` and coalesces adjacent free space. |
| **`r`** | `r` | Resets the heap back to its initial empty state. |
| **`q`** | `q` | Exits the program cleanly. |

---

## 🏗️ Architecture & Mechanics

- **Boundary Tag Allocator**: Each block uses a 24B header (`node_t`) + payload + 8B footer (`footer_t`), giving a fixed 32B block overhead.
- **Segregated Bins**: Free blocks are indexed into 9 size-classed doubly-linked list bins.
- **Zero-Dependency Core**: Managed heap resides in a static buffer without touching stdlib system `malloc`/`free`.

