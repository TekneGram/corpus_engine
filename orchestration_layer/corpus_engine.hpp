#pragma once

#include <string>

#include "progress_emitter.hpp"

namespace teknegram {

    class CorpusEngine {
        public:
            void run(const std::string& model_path,
                    const std::string& input_text_path,
                    const std::string& output_dir,
                    const std::string& semantics_rules_path = std::string(),
                    const ProgressEmitter* progress_emitter = 0) const;
    };
}
