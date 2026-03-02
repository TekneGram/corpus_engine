Think first about how a CPU actually reads memory. The processor does
not read individual integers directly from RAM one by one. Instead, it
loads small blocks of nearby memory into a fast intermediate storage
area called the cache. This block is called a cache line. A typical
cache line is 64 bytes. When the CPU requests one number, it quietly
loads the surrounding 63 bytes as well. This behaviour is fundamental to
everything that follows.

Now imagine memory as a long bookshelf. If you read words from a book
that is arranged in order, your hand slides smoothly from left to right
across the page. That is sequential memory access. If instead each word
you need is located in a different book scattered randomly across the
room, you must constantly stand up, walk somewhere else, and open a
different book. That is random access. The second case is far slower,
not because reading is slow, but because moving is slow.

Sequential memory access means that the program reads memory locations
that are next to each other. In the sparse matrix entries file, the
feature_id and count pairs for a document are stored contiguously. When
the CPU reads one entry, the next entries are already sitting in the
same cache line. The processor does not need to fetch new memory from
RAM. It simply continues reading from what is already loaded. This is
like reading a paragraph instead of jumping between different chapters
in different books.

Cache locality refers to how well a program uses the cache. There are
two types: spatial locality and temporal locality. Spatial locality
means that if you access one memory location, you will likely access
nearby locations soon. The sparse matrix layout has excellent spatial
locality because document entries are stored in one continuous block.
Temporal locality means that recently accessed data is likely to be
accessed again soon. In Monte Carlo sampling, the accumulator vector is
reused repeatedly, so it remains hot in cache. Together, these
properties mean that the CPU spends most of its time computing rather
than waiting.

Now consider what O(1) document access means. In the offsets file, each
document's data range can be computed immediately using two numbers:
offsets\[d\] and offsets\[d+1\]. This is like having a table of contents
where each chapter lists its exact page range. You do not need to scan
the book to find where a chapter begins. You look at one index entry and
you know instantly. The time to locate a document's feature list does
not depend on how large the corpus is. It depends only on one array
lookup. That is constant time.

Next, consider dynamic allocation. Dynamic allocation means asking the
operating system for memory while the program is running. This involves
bookkeeping, possible memory fragmentation, and pointer chasing. It
introduces unpredictability. In contrast, the sparse matrix uses
precomputed flat arrays. All memory is allocated once and then reused.
This is like building a warehouse with fixed shelves rather than
constantly constructing new storage boxes while working. The absence of
dynamic allocation reduces overhead and improves predictability.

Finally, consider what it means to be SIMD-friendly. SIMD stands for
Single Instruction, Multiple Data. Modern CPUs can perform the same
operation on several integers at once using vector registers. This works
best when data is stored in uniform, tightly packed structures. In the
entries file, each entry has the same size and layout. The CPU can load
multiple feature_id or count values into vector registers and process
them together. If the data were scattered through linked lists or
compressed with variable-length encoding, vectorization would be
extremely difficult. The flat 8-byte structure is like stacking
identical bricks; the machine can grab several bricks at once. If
instead every brick were a different shape, this would not be possible.

All of these properties arise from one design principle: memory should
be arranged in the order in which it will be consumed. The sparse matrix
is arranged document-first because Monte Carlo simulation consumes
documents sequentially. The layout mirrors the computation. When layout
and computation agree, the machine moves smoothly. When they disagree,
the machine stalls waiting for memory.

Good. Let us walk through this slowly, as if we are watching the
processor think.

We begin with the metadata file. This file is small, only 64 bytes, and
it is read once at the beginning. It is like opening the cover of a book
to read the title, the edition number, and the total number of chapters
before you begin reading. The program loads this header into memory and
now knows three essential facts: how many documents exist, how many
features exist, and how many total entries are stored.

Conceptually in memory it looks like this:

    lemma.spm.meta.bin  (64 bytes)

    +--------------------------------------------------------------+
    | magic | version | num_docs | num_features | num_nonzero | ... |
    +--------------------------------------------------------------+

Once this is loaded, the simulation allocates an accumulator vector of
size num_features. This vector will hold the running totals during
sampling. It sits in memory like a long row of empty buckets waiting to
collect counts.

    accumulator[F]

    +----+----+----+----+----+----+----+----+----+----+ ...
    |  0 |  0 |  0 |  0 |  0 |  0 |  0 |  0 |  0 |  0 |
    +----+----+----+----+----+----+----+----+----+----+

Now the offsets file is memory-mapped. This file is simply an array of
uint64 values. It tells us where each document's features begin and end
inside the entries file. It is like a railway timetable that lists the
starting and ending platform for each train.

In memory it appears as:

    lemma.spm.offsets.bin

    +----+----+----+----+----+
    |  0 |  3 |  6 |  9 | 12 |
    +----+----+----+----+----+
      d0   d1   d2   d3   d4

If we have four documents, then offsets\[0\] is 0 and offsets\[4\] is
12. This means there are 12 total entries. Each pair of adjacent numbers
defines one document's range.

Next, the entries file is memory-mapped. This file is a long flat array
of feature_id and count pairs. Imagine it as a warehouse aisle filled
with identical boxes. Each box contains exactly two numbers. There are
no separators between documents. Everything is continuous.

    lemma.spm.entries.bin

    Index:   0        1        2        3        4        5
            +--------+--------+--------+--------+--------+--------+
            | f0,c1  | f4,c2  | f7,c1  | f2,c3  | f8,c1  | f1,c5  |
            +--------+--------+--------+--------+--------+--------+

Now let us simulate a first Monte Carlo run. Suppose we randomly sample
documents 1 and 3.

The simulation begins by looking at the offsets table. For document 1,
it reads offsets\[1\] and offsets\[2\].

    offsets:

    +----+----+----+----+----+
    |  0 |  3 |  6 |  9 | 12 |
    +----+----+----+----+----+
            ^    ^
           d1   d2

This tells us that document 1's entries lie between index 3 and index 6
in the entries file. The processor does not scan. It does not search. It
simply jumps to position 3. This is like opening directly to page 45
because the table of contents told you exactly where the chapter begins.

In the entries file, the CPU now reads entries 3, 4, and 5 sequentially:

    entries:

    Index:   0        1        2        3        4        5
            +--------+--------+--------+--------+--------+--------+
            | f0,c1  | f4,c2  | f7,c1  | f2,c3  | f8,c1  | f1,c5  |
            +--------+--------+--------+--------+--------+--------+
                                          ^        ^        ^
                                        read     read     read

Because these entries are contiguous, when the CPU loads entry 3, it
likely also loads entries 4 and 5 into the cache automatically. This is
like pulling one drawer open and finding that all the next items are
already in reach.

For each entry, the simulation performs:

accumulator\[feature_id\] += count.

If entry 3 is (f2, c3), then bucket 2 in the accumulator increases by 3.
If entry 4 is (f8, c1), bucket 8 increases by 1. The accumulator begins
to fill like a row of jars receiving coins.

Now the simulation moves to document 3. Again it consults the offsets:

    offsets:

    +----+----+----+----+----+
    |  0 |  3 |  6 |  9 | 12 |
    +----+----+----+----+----+
                     ^    ^
                    d3   d4

Document 3 lies between index 9 and 12. The processor jumps directly
there and reads those entries sequentially. Again the memory access is
smooth. It is like walking down a straight corridor instead of
navigating a maze.

At the end of this run, the accumulator contains the combined feature
counts of documents 1 and 3. No other memory was touched. The simulation
did not inspect documents 0 or 2. It did not scan the whole entries
array. It read only the regions specified by the offsets table.

Now imagine a second run. Suppose this time we sample documents 0, 2,
and 3. The same process repeats. The offsets file acts as a precise map.
The entries file acts as a warehouse. The accumulator acts as a ledger.
The CPU walks directly to each required aisle, reads a contiguous block,
and updates the ledger.

The key point is that the layout mirrors the way the simulation thinks.
The simulation thinks in documents. Therefore, memory is arranged in
documents. When thought and structure align, computation flows like
water through a straight channel. When they do not align, the processor
must jump unpredictably, and every jump is a delay.

In this design, the metadata tells us the scale of the landscape, the
offsets file tells us where each field begins and ends, and the entries
file stores the actual harvest. A Monte Carlo run is simply walking to
selected fields and gathering their crops into a single basket.
