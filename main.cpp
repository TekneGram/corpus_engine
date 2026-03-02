#include <iostream>
#include <string>

#include "orchestration_layer/corpus_engine.hpp"

int main(int argc, char** argv) {
    const std::string model_path = (argc > 1)
        ? argv[1]
        : "models/english-partut-ud-2.5-191206.udpipe";

    const std::string input_text_path = (argc > 2)
        ? argv[2]
        : "README.md";

    const std::string output_dir = (argc > 3)
        ? argv[3]
        : "corpus_output";
    const std::string semantics_rules_path = (argc > 4)
        ? argv[4]
        : "";

    try {
        teknegram::CorpusEngine engine;
        engine.run(model_path, input_text_path, output_dir, semantics_rules_path);
        std::cout << "Corpus pipeline completed. Output: " << output_dir << "\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
