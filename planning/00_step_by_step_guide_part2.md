# Step by step guide to building the corpus - Part 2

Part 1 focused on building the corpus representation layer in binary files.

Part 2 focuses on building the corpus search layer

## Building an inverted index
Inverted indexes are built for words, lemmas, and pos.

For build_index_from_file(...) on word.bin (raw mode), outputs are:

  1. word.index.positions
  2. word.index.header

  Using tokens:

  - Sentence 1: small cats chase quick mice
  - Sentence 2: small cats eat fresh fish today

  Assume word IDs (first-seen):

  - small=0, cats=1, chase=2, quick=3, mice=4, eat=5, fresh=6, fish=7, today=8

  So input word.bin values by token position are:

  pos:    0 1 2 3 4 5 6 7 8 9 10
  wordID: 0 1 2 3 4 0 1 5 6 7 8

  ### Conceptual postings built by algorithm

  0 -> [0,5]
  1 -> [1,6]
  2 -> [2]
  3 -> [3]
  4 -> [4]
  5 -> [7]
  6 -> [8]
  7 -> [9]
  8 -> [10]

  ### word.index.positions

  Flattened postings in ID order:

  [0,5, 1,6, 2, 3, 4, 7, 8, 9, 10]

  ### Hex bytes (uint32 little-endian)

  00 00 00 00 05 00 00 00
  01 00 00 00 06 00 00 00
  02 00 00 00
  03 00 00 00
  04 00 00 00
  07 00 00 00
  08 00 00 00
  09 00 00 00
  0A 00 00 00

  ———

  ## word.index.header

  Layout:

  - first uint32 header_size (= number of IDs) -> 9
  - then 9 entries of (uint64 offset, uint32 length)

  Offsets/lengths (in bytes into word.index.positions):

  id0: offset=0,  length=8
  id1: offset=8,  length=8
  id2: offset=16, length=4
  id3: offset=20, length=4
  id4: offset=24, length=4
  id5: offset=28, length=4
  id6: offset=32, length=4
  id7: offset=36, length=4
  id8: offset=40, length=4

  ### Hex bytes

  Header size (uint32):

  09 00 00 00

  Entries (uint64 offset + uint32 length, little-endian):

  00 00 00 00 00 00 00 00 08 00 00 00
  08 00 00 00 00 00 00 00 08 00 00 00
  10 00 00 00 00 00 00 00 04 00 00 00
  14 00 00 00 00 00 00 00 04 00 00 00
  18 00 00 00 00 00 00 00 04 00 00 00
  1C 00 00 00 00 00 00 00 04 00 00 00
  20 00 00 00 00 00 00 00 04 00 00 00
  24 00 00 00 00 00 00 00 04 00 00 00
  28 00 00 00 00 00 00 00 04 00 00 00
