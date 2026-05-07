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
