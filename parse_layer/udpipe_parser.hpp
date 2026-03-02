#pragma once

#include <memory>
#include <string>

#include "parsed_document.hpp"

namespace ufal {
    namespace udpipe {
        class model;
    }
}

namespace teknegram {

    class UDPipeParser {
        public:
            explicit UDPipeParser(const std::string& model_path);
            ~UDPipeParser();

            ParsedDocument parse(const std::string& text, DocId document_id) const;

        private:
            PosId to_pos_id(const std::string& upos) const;
            DeprelId to_deprel_id(const std::string& deprel) const;

            std::unique_ptr<ufal::udpipe::model> model_;
    };
}
