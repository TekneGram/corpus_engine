#include "dependency_index_builder.hpp"

#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

#include "../corpus_shared_layer/artifact_builders.hpp"

namespace teknegram {

    void DependencyIndexBuilder::build(const std::string& corpus_dir,
                                       PostingEncodingMode mode) const {
        std::ifstream head_in((corpus_dir + "/head.bin").c_str(), std::ios::binary | std::ios::in);
        if (!head_in) {
            throw std::runtime_error("Failed to open head.bin for dependency index build.");
        }

        std::vector<std::vector<std::uint32_t> > children_by_head;
        std::uint32_t head = 0;
        std::uint32_t child_pos = 0;

        while (head_in.read(reinterpret_cast<char*>(&head), sizeof(head))) {
            if (head != std::numeric_limits<std::uint32_t>::max()) {
                const std::size_t idx = static_cast<std::size_t>(head);
                if (children_by_head.size() <= idx) {
                    children_by_head.resize(idx + 1);
                }
                children_by_head[idx].push_back(child_pos);
            }
            ++child_pos;
        }

        WritePositionIndex(corpus_dir + "/dep.index.header",
                           corpus_dir + "/dep.index.positions",
                           children_by_head,
                           mode);
    }

} // namespace corpus
