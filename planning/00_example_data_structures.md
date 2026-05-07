# Example Data Structures (Word + 4gram)

This is a synthetic demonstration corpus (not real binary bytes), designed to show what key artifacts look like when serialized.

Assumptions:
- 8 documents
- 2 sentences per document
- 3 subcorpora: docs `[0,1,2]`, `[3,4,5]`, `[6,7]`
- `postingFormat = raw` (so index/docfreq entries are uncompressed)
- 4-gram builder behavior matches code (`N==4` keeps only features with frequency >= 2)

## Corpus Text (short, varied sentence lengths)

- doc0:
  - `small cats chase quick mice` (5)
  - `small cats eat fresh fish today` (6)
- doc1:
  - `young dogs chase small cats` (5)
  - `young dogs eat fresh bones` (5)
- doc2:
  - `bright birds chase tiny insects` (5)
  - `bright birds eat fresh fish today` (6)
- doc3:
  - `students read long books` (4)
  - `students write clear notes daily` (5)
- doc4:
  - `teachers read student essays daily` (5)
  - `teachers grade long essays with care` (6)
- doc5:
  - `writers read rough drafts nightly` (5)
  - `writers eat fresh fish today` (5)
- doc6:
  - `engines use clean fuel daily` (5)
  - `engines need more oil soon` (5)
- doc7:
  - `motors need more oil soon` (5)
  - `motors use clean fuel` (4)

Total tokens = 81.

---

## Dictionary Used for `word.bin` / `word.lexicon.bin`

Assigned in first-seen order:

- `0 small`
- `1 cats`
- `2 chase`
- `3 quick`
- `4 mice`
- `5 eat`
- `6 fresh`
- `7 fish`
- `8 today`
- `9 young`
- `10 dogs`
- `11 bones`
- `12 bright`
- `13 birds`
- `14 tiny`
- `15 insects`
- `16 students`
- `17 read`
- `18 long`
- `19 books`
- `20 write`
- `21 clear`
- `22 notes`
- `23 daily`
- `24 teachers`
- `25 student`
- `26 essays`
- `27 grade`
- `28 with`
- `29 care`
- `30 writers`
- `31 rough`
- `32 drafts`
- `33 nightly`
- `34 engines`
- `35 use`
- `36 clean`
- `37 fuel`
- `38 need`
- `39 more`
- `40 oil`
- `41 soon`
- `42 motors`

Conceptual `word.lexicon.bin`:

```text
count = 43
[0]="small", [1]="cats", [2]="chase", ..., [42]="motors"
```

---

## `word.bin`

Global token stream of word IDs:

```text
[0,1,2,3,4,0,1,5,6,7,8,
 9,10,2,0,1,9,10,5,6,11,
 12,13,2,14,15,12,13,5,6,7,8,
 16,17,18,19,16,20,21,22,23,
 24,17,25,26,23,24,27,18,26,28,29,
 30,17,31,32,33,30,5,6,7,8,
 34,35,36,37,23,34,38,39,40,41,
 42,38,39,40,41,42,35,36,37]
```

---

## `sentence_bounds.bin`

Global token start per sentence:

```text
[0,5, 11,16, 21,26, 32,36, 41,46, 52,57, 62,67, 72,77]
```

(16 starts = 8 docs * 2 sentences)

---

## `doc_ranges.bin`

Document token ranges `(start,end)`:

```text
doc0: (0,11)
doc1: (11,21)
doc2: (21,32)
doc3: (32,41)
doc4: (41,52)
doc5: (52,62)
doc6: (62,72)
doc7: (72,81)
```

Flat stored representation:

```text
[0,11, 11,21, 21,32, 32,41, 41,52, 52,62, 62,72, 72,81]
```

---

## `word_doc.bin`

Doc ID per token position (81 entries):

```text
[0,0,0,0,0,0,0,0,0,0,0,
 1,1,1,1,1,1,1,1,1,1,
 2,2,2,2,2,2,2,2,2,2,2,
 3,3,3,3,3,3,3,3,3,
 4,4,4,4,4,4,4,4,4,4,4,
 5,5,5,5,5,5,5,5,5,5,
 6,6,6,6,6,6,6,6,6,6,
 7,7,7,7,7,7,7,7,7]
```

---

## `word.index.header`

For raw postings, each header entry is:
- `offset` = byte offset inside `word.index.positions`
- `length` = number of bytes for that word’s postings (`count * 4`)

Header size = 43 (word IDs 0..42)

```text
id: (offset,length)
0:  (0,12)
1:  (12,12)
2:  (24,12)
3:  (36,4)
4:  (40,4)
5:  (44,16)
6:  (60,16)
7:  (76,12)
8:  (88,12)
9:  (100,8)
10: (108,8)
11: (116,4)
12: (120,8)
13: (128,8)
14: (136,4)
15: (140,4)
16: (144,8)
17: (152,12)
18: (164,8)
19: (172,4)
20: (176,4)
21: (180,4)
22: (184,4)
23: (188,12)
24: (200,8)
25: (208,4)
26: (212,8)
27: (220,4)
28: (224,4)
29: (228,4)
30: (232,8)
31: (240,4)
32: (244,4)
33: (248,4)
34: (252,8)
35: (260,8)
36: (268,8)
37: (276,8)
38: (284,8)
39: (292,8)
40: (300,8)
41: (308,8)
42: (316,8)
```

---

## `word.index.positions`

Raw postings lists (token positions) by word ID:

```text
0  -> [0,5,14]
1  -> [1,6,15]
2  -> [2,13,23]
3  -> [3]
4  -> [4]
5  -> [7,18,28,58]
6  -> [8,19,29,59]
7  -> [9,30,60]
8  -> [10,31,61]
9  -> [11,16]
10 -> [12,17]
11 -> [20]
12 -> [21,26]
13 -> [22,27]
14 -> [24]
15 -> [25]
16 -> [32,36]
17 -> [33,42,53]
18 -> [34,48]
19 -> [35]
20 -> [37]
21 -> [38]
22 -> [39]
23 -> [40,45,66]
24 -> [41,46]
25 -> [43]
26 -> [44,49]
27 -> [47]
28 -> [50]
29 -> [51]
30 -> [52,57]
31 -> [54]
32 -> [55]
33 -> [56]
34 -> [62,67]
35 -> [63,78]
36 -> [64,79]
37 -> [65,80]
38 -> [68,73]
39 -> [69,74]
40 -> [70,75]
41 -> [71,76]
42 -> [72,77]
```

## Interpretation
word.index.positions is the inverted (reverse) index payload.
  word.index.header is the table-of-contents for that payload.

  Concrete example:

  - Suppose word_id=7 (“fish”).
  - In word.index.header, entry 7 is:
      - offset=76
      - length=12

  Because posting format is raw uint32 positions, 12 bytes = 3 positions.

  Lookup flow:

  1. Read header entry for ID 7.
  2. Seek to byte 76 in word.index.positions.
  3. Read 12 bytes.
  4. Interpret as 3 uint32 values -> e.g. [9, 30, 60].

  Meaning: token positions 9, 30, and 60 in word.bin are the word “fish”.

  So:

  - word.bin answers: “what word is at position i?”
  - word.index.positions + word.index.header answers: “at which positions does word_id k occur?”

---

## 4-gram extraction note

Because the builder filters 4-grams with frequency `< 2`, only repeated 4-grams survive in `4gram.*` outputs.

Repeated 4-grams in this corpus:
- `[5,6,7,8]` (`eat fresh fish today`) appears 3 times
- `[38,39,40,41]` (`need more oil soon`) appears 2 times

All other 4-grams are dropped.

---

## `4gram.lexicon.bin`

Conceptual kept feature lexicon (word-ID tuples):

```text
feature_count = 2
feature 0 -> [5,6,7,8]
feature 1 -> [38,39,40,41]
```

---

## `4gram.freq.bin`

Frequency per kept feature:

```text
[3,2]
```

---

## `4gram.docfreq.header`

Raw docfreq entry size is 8 bytes (`DocFreqEntry{doc_id,count}`).

- feature 0 postings: 3 docs -> 24 bytes
- feature 1 postings: 2 docs -> 16 bytes

```text
header_size = 2
0: (offset=0,  length=24)
1: (offset=24, length=16)
```

(Conceptual `4gram.docfreq.entries` would be:
- feature 0: `(0,1),(2,1),(5,1)`
- feature 1: `(6,1),(7,1)`)

---

## `4gramspm.meta.bin` (same content intent as `4gram.spm.meta.bin`)

Sparse matrix metadata for 4-gram rows:

```text
magic            = 0x53504D31  ("SPM1")
version          = 1
num_docs         = 8
num_features     = 2
num_nonzero      = 5
entry_size_bytes = 8   // (feature_id:uint32, count:uint32)
reserved         = 0
padding          = 24 zero bytes
```

---

## `4gram.spm.offsets.bin`

Doc-row offsets into flat `4gram.spm.entries.bin`:

Rows are:
- doc0: `[(0,1)]`
- doc1: `[]`
- doc2: `[(0,1)]`
- doc3: `[]`
- doc4: `[]`
- doc5: `[(0,1)]`
- doc6: `[(1,1)]`
- doc7: `[(1,1)]`

Offsets:

```text
[0,1,1,2,2,2,3,4,5]
```

---

## `4gram.spm.entries.bin`

Flat sparse entries in document order:

```text
[(0,1), (0,1), (0,1), (1,1), (1,1)]
```

---

## `word.spm.meta.bin`

Sparse matrix metadata for word rows:

```text
magic            = 0x53504D31
version          = 1
num_docs         = 8
num_features     = 43
num_nonzero      = 69
entry_size_bytes = 8
reserved         = 0
padding          = 24 zero bytes
```

---

## `word.spm.offsets.bin`

Per-doc offsets into flat `word.spm.entries.bin`:

```text
[0,9,17,26,34,43,52,61,69]
```

---

## `word.spm.entries.bin`

Flat `(feature_id,count)` rows in doc order.

doc0 (9 entries):
```text
(0,2),(1,2),(2,1),(3,1),(4,1),(5,1),(6,1),(7,1),(8,1)
```

doc1 (8 entries):
```text
(0,1),(1,1),(2,1),(5,1),(6,1),(9,2),(10,2),(11,1)
```

doc2 (9 entries):
```text
(2,1),(5,1),(6,1),(7,1),(8,1),(12,2),(13,2),(14,1),(15,1)
```

doc3 (8 entries):
```text
(16,2),(17,1),(18,1),(19,1),(20,1),(21,1),(22,1),(23,1)
```

doc4 (9 entries):
```text
(17,1),(18,1),(23,1),(24,2),(25,1),(26,2),(27,1),(28,1),(29,1)
```

doc5 (9 entries):
```text
(5,1),(6,1),(7,1),(8,1),(17,1),(30,2),(31,1),(32,1),(33,1)
```

doc6 (9 entries):
```text
(23,1),(34,2),(35,1),(36,1),(37,1),(38,1),(39,1),(40,1),(41,1)
```

doc7 (8 entries):
```text
(35,1),(36,1),(37,1),(38,1),(39,1),(40,1),(41,1),(42,2)
```

Note: These rows are conceptual; the binary stores them as a single contiguous array of `(uint32 feature_id, uint32 count)` records.
