#include "build_options.hpp"

#include <stdexcept>

namespace teknegram {

    PostingEncodingMode ParsePostingEncodingMode(const std::string& value) {
        if (value.empty() || value == "raw") {
            return PostingEncodingMode::kRaw;
        }
        if (value == "compressed") {
            return PostingEncodingMode::kCompressed;
        }
        throw std::runtime_error("Unknown posting encoding mode: " + value);
    }

    std::string PostingEncodingModeToString(PostingEncodingMode mode) {
        if (mode == PostingEncodingMode::kCompressed) {
            return "compressed";
        }
        return "raw";
    }

} // namespace teknegram
