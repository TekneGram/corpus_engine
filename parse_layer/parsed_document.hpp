#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../corpus_shared_layer/corpus_types.hpp"

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
