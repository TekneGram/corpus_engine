#include "dependency_index_builder.hpp"

#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

#include "../corpus_shared_layer/binary_header.hpp"

namespace teknegram {

    void DependencyIndexBuilder::build(const std::string& corpus_dir) const {
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

        std::vector<IndexHeaderEntry> header(children_by_head.size());
        std::uint64_t offset = 0;
        for (std::size_t i = 0; i < children_by_head.size(); ++i) {
            header[i].offset = offset;
            header[i].length = static_cast<std::uint32_t>(children_by_head[i].size());
            offset += static_cast<std::uint64_t>(children_by_head[i].size());
        }

        std::ofstream header_out((corpus_dir + "/dep.index.header").c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
        std::ofstream positions_out((corpus_dir + "/dep.index.positions").c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
        if (!header_out || !positions_out) {
            throw std::runtime_error("Failed to open dependency index outputs.");
        }

        const std::uint32_t size = static_cast<std::uint32_t>(header.size());
        header_out.write(reinterpret_cast<const char*>(&size), sizeof(size));
        if (!header.empty()) {
            header_out.write(reinterpret_cast<const char*>(header.data()),
                            static_cast<std::streamsize>(header.size() * sizeof(IndexHeaderEntry)));
        }

        for (std::size_t i = 0; i < children_by_head.size(); ++i) {
            if (!children_by_head[i].empty()) {
                positions_out.write(reinterpret_cast<const char*>(children_by_head[i].data()),
                                    static_cast<std::streamsize>(children_by_head[i].size() * sizeof(std::uint32_t)));
            }
        }
    }

} // namespace corpus
