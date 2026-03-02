#pragma once

#include <cstdint>
#include <string>

namespace teknegram {

    class MetadataWriter {
        public:
            explicit MetadataWriter(const std::string& db_path);

            void upsert_document(std::uint32_t document_id,
                                const std::string& title,
                                const std::string& author,
                                const std::string& relative_path);

            void upsert_folder_segment(std::uint32_t segment_id,
                                       const std::string& segment_text);

            void upsert_document_segment(std::uint32_t document_id,
                                         std::uint32_t depth,
                                         std::uint32_t segment_id);

            void upsert_semantic_key(std::uint32_t key_id,
                                     const std::string& key_name);

            void upsert_semantic_value(std::uint32_t value_id,
                                       std::uint32_t key_id,
                                       const std::string& value_text);

            void upsert_document_group(std::uint32_t document_id,
                                       std::uint32_t key_id,
                                       std::uint32_t value_id);

        private:
            std::string db_path_;
    };
}
