#pragma once

#include <string>

namespace teknegram {

    enum class PostingEncodingMode {
        kRaw,
        kCompressed
    };

    struct BuildOptions {
        PostingEncodingMode posting_encoding;
        bool emit_ngram_positions;

        BuildOptions()
            : posting_encoding(PostingEncodingMode::kRaw),
              emit_ngram_positions(true) {}
    };

    PostingEncodingMode ParsePostingEncodingMode(const std::string& value);
    std::string PostingEncodingModeToString(PostingEncodingMode mode);

} // namespace teknegram
