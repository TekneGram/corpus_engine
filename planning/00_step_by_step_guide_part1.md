# Step by step guide to building the corpus representation - Part 1

## corpus_engine
- The engine instantiates a number of important objects:
    - DictionaryBuilder
    - StructuralLayer
    - CoreTokenLayer
    - MetadataWriter

- Each document is parsed with the udpipe parser one document at a time.
- The parsed document struct is passed into the CoreTokenLayer object along with the DictionaryBuilder and StructuralLayer objects

```cpp
const ParsedDocument doc = parser.parse(text, document_id);
// ...
core_token_layer.append_document(doc, &dictionary_builder, $structural_layer)
```

### Parsed Document
Defined in parsed_document.hpp

```cpp
namespace teknegram {

struct ParsedToken {
    std::string word;
    std::string lemma;
    PosId pos_id;
    std::uint32_t head; // sentence-local head, 0 means root
    DeprelId deprel_id;
};

struct ParsedDocument {
    std::vector<ParsedToken> tokens;
    std::vector<std::uint32_t> sentence_starts;
    DocId document_id;
};

}
```

`PosId`, `DeprelId` and `DocId` are defined as `std::uint8_t`, `std::uint8_t` and `std::uint32_t` respectively, using fixed width 8 bit unsigned integers for parts of speech and dependency relationsince as there are fewer of them (range 0 - 255) and 32 bit unsigned integers for document ids as there are possibly many of them (range 0 - 4,292,967,295).

Note: if an id is assigned 200, it is represented in binary as 11001000 which can be broken down into 1100 = C in hex, and 1000 = 8 in hex, so the raw byte is represented as C8.

Example:

```cpp
ParsedDocument doc0;
  doc0.document_id = 0;
  doc0.sentence_starts = {0, 5};  // sentence 1 starts at token 0, sentence 2 at token 5

  doc0.tokens = {
    // sentence 1: "small cats chase quick mice"
    ParsedToken{"small", "small", 1, 2, 4},   // ADJ -> amod(cats)
    ParsedToken{"cats",  "cat",   8, 3, 27},  // NOUN -> nsubj(chase)
    ParsedToken{"chase", "chase", 16, 0, 35}, // VERB -> root
    ParsedToken{"quick", "quick", 1, 5, 4},   // ADJ -> amod(mice)
    ParsedToken{"mice",  "mouse", 8, 3, 29},  // NOUN -> obj(chase)

    // sentence 2: "small cats eat fresh fish today"
    ParsedToken{"small", "small", 1, 2, 4},   // ADJ -> amod(cats)
    ParsedToken{"cats",  "cat",   8, 3, 27},  // NOUN -> nsubj(eat)
    ParsedToken{"eat",   "eat",   16, 0, 35}, // VERB -> root
    ParsedToken{"fresh", "fresh", 1, 5, 4},   // ADJ -> amod(fish)
    ParsedToken{"fish",  "fish",  8, 3, 29},  // NOUN -> obj(eat)
    ParsedToken{"today", "today", 3, 3, 3}    // ADV -> advmod(eat)
  };
```
- sentence_starts are token offsets within this document.
- head is sentence-local, 1-based; 0 means root.
- pos_id/deprel_id are numeric IDs from the UDPipe mapping used in udpipe_parser.cpp.

### Processing the parsed document in core_token_layer.cpp

```cpp
std::uint32_t current_sentence_start = doc.sentence_starts.empty() ? 0U : doc.sentence_starts[0];
std::uint32_t next_sentence_start = doc.sentence_starts.size() > 1 ? doc.sentence_starts[1] : static_cast<std::uint32_t>(doc.tokens.size());
```
We loop through sentences, processing one sentence at a time:
  - current_sentence_start
      - If there are no sentence boundaries, use 0.
      - Otherwise use the first sentence start (doc.sentence_starts[0]).
  - next_sentence_start
      - If there is a second sentence boundary, use it (doc.sentence_starts[1]).
      - Otherwise use doc.tokens.size() (treat the whole doc as one sentence ending at last token).

We loop through each token in each sentence. For example:
```cpp
const std::uint32_t word_id = dictionary_builder->get_word_id(token.word);
```
This gets the id for the word, or creates it if it doesn't exist. This is essentially building the dictionary as we process the document.

```cpp
std::uint32_t global_head = static_cast<std::uint32_t>(-1);
            if (token.head != 0U) {
                const std::uint32_t local_head_zero_based = token.head - 1U;
                const std::uint32_t local_head_global_in_doc = current_sentence_start + local_head_zero_based;
                global_head = token_start + local_head_global_in_doc;
            }
```
The above ensures that udpipe's sentence-local head index is converted into a global head for the whole corpus. This is because the corpus is stored as one global token stream across all documents, so if heads stay local, then lookup would be ambiguous.

```cpp
word_out_.write(reinterpret_cast<const char*>(&word_id), sizeof(word_id));
lemma_out_.write(reinterpret_cast<const char*>(&lemma_id), sizeof(lemma_id));
pos_out_.write(reinterpret_cast<const char*>(&token.pos_id), sizeof(token.pos_id));
head_out_.write(reinterpret_cast<const char*>(&global_head), sizeof(global_head));
deprel_out_.write(reinterpret_cast<const char*>(&token.deprel_id), sizeof(token.deprel_id));
```
The above writes out to word.bin, lemma.bin, pos.bin, head.bin, deprel.bin.

For example:

  - Sentence 1: small cats chase quick mice
  - Sentence 2: small cats eat fresh fish today

  And the example dictionary IDs we used:

  - small->0, cats->1, chase->2, quick->3, mice->4, eat->5, fresh->6, fish->7, today->8

  After all doc0 tokens are processed, word_out_ has appended (in order):

  [0, 1, 2, 3, 4, 0, 1, 5, 6, 7, 8]

  As raw bytes in word.bin (little-endian uint32_t), that doc0 segment is:

  00 00 00 00
  01 00 00 00
  02 00 00 00
  03 00 00 00
  04 00 00 00
  00 00 00 00
  01 00 00 00
  05 00 00 00
  06 00 00 00
  07 00 00 00
  08 00 00 00

Example of pos.bin:

 For the same doc0 example POS IDs:

  - sentence 1: small(ADJ=1) cats(NOUN=8) chase(VERB=16) quick(ADJ=1) mice(NOUN=8)
  - sentence 2: small(1) cats(8) eat(16) fresh(1) fish(8) today(ADV=3)

  pos_out_ appends:

  [1, 8, 16, 1, 8, 1, 8, 16, 1, 8, 3]

  Because PosId is uint8_t, each is 1 byte.
  Raw bytes (hex) would be:

  01 08 10 01 08 01 08 10 01 08 03

  So pos.bin is a compact byte stream: one POS byte per token position.

### Adding structural data
Inside `core_token_layer.append_document` we have the following call to the StructuralLayer object:
```cpp
structural_layer->append_document(doc.document_id,
                                        token_start,
                                        token_end,
                                        doc.sentence_starts,
                                        token_start);
```

This creates structural information about the corpus in the following binary outputs:

#### **word_doc.bin**
This maps each global token position to its owning document ID.

  Purpose:

  - lets downstream steps answer “this token position belongs to which doc?”
  - used by n-gram/docfreq builders to aggregate per-document counts.

  For the doc0 example above (token_start=0, token_end=11, doc_id=0), its segment in word_doc.bin is:

  [0,0,0,0,0,0,0,0,0,0,0]

  (one 0 per token position 0..10).

Data is stored as follows:
One uint32 doc ID per token position.

  Logical values (81 total):

  [0 x11 times, 1 x10 times, 2 x11 times, 3 x9 times, 4 x11 times, 5 x10 times, 6 x10 times, 7 x9 times]

  Expanded start:

  [0,0,0,0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1,1,1, ...]

  Byte examples:

  - doc0 token entries (0): 00 00 00 00 repeated
  - then doc1 entries (1): 01 00 00 00 repeated

  Start of file:

  00 00 00 00 00 00 00 00 00 00 00 00 ... (11 times)
  
  01 00 00 00 01 00 00 00 ... (10 times)

#### **sentence_bounds.bin**
sentence_bounds.bin stores the global token start position of every sentence in corpus order.

  Purpose:

  - marks sentence boundaries so algorithms (like n-gram extraction) don’t cross sentence edges.
  - lets you reconstruct sentence spans from the global token stream.

  Example:
  If sentence starts are [0, 5, 11, 16, ...], then:

  - sentence 0 = tokens [0..4]
  - sentence 1 = tokens [5..10]
  - sentence 2 = tokens [11..15]
    (and the next start marks each end).

Data is stored as follows:
One uint32 global start per sentence.

  Logical values:

  [0,5, 11,16, 21,26, 32,36, 41,46, 52,57, 62,67, 72,77]

  First bytes:

  - 0  -> 00 00 00 00
  - 5  -> 05 00 00 00
  - 11 -> 0B 00 00 00
  - 16 -> 10 00 00 00

  Start of file:

  00 00 00 00 05 00 00 00 0B 00 00 00 10 00 00 00 ...

#### **doc_ranges.bin**
doc_ranges.bin stores each document’s global token span as (start, end) pairs.

  Purpose:

  - marks document boundaries in the global token stream.
  - lets downstream code iterate tokens per document quickly.

  Example:

  doc0: (0,11)
  doc1: (11,21)
  doc2: (21,32)
  ...

  So for document d, its tokens are positions [start, end) (end-exclusive).

Data is stored as follows:
 Pairs of uint32 (start,end) per doc.

  Logical values:

  [(0,11), (11,21), (21,32), (32,41), (41,52), (52,62), (62,72), (72,81)]

  Flattened uint32 sequence:

  [0,11, 11,21, 21,32, 32,41, 41,52, 52,62, 62,72, 72,81]

  First bytes (little-endian):

  - 0  -> 00 00 00 00
  - 11 -> 0B 00 00 00
  - 11 -> 0B 00 00 00
  - 21 -> 15 00 00 00

  So beginning of file:

  00 00 00 00 0B 00 00 00 0B 00 00 00 15 00 00 00 ...
