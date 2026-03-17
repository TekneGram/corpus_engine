#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

#include "orchestration_layer/corpus_engine.hpp"
#include "orchestration_layer/progress_emitter.hpp"
#include "corpus_shared_layer/build_options.hpp"
#include "third_party/nlohmann/json.hpp"

namespace {

using json = nlohmann::json;

void EmitJson(const json& payload);

class JsonProgressEmitter : public teknegram::ProgressEmitter {
    public:
        virtual void emit(const std::string& message, int percent) const {
            EmitJson(json{
                {"type", "progress"},
                {"message", message},
                {"percent", percent}
            });
        }
};

bool HasNonWhitespace(const std::string& value) {
    for (std::string::size_type i = 0; i < value.size(); ++i) {
        const char ch = value[i];
        if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t') {
            return true;
        }
    }
    return false;
}

std::string ReadStdin() {
    return std::string(
        std::istreambuf_iterator<char>(std::cin),
        std::istreambuf_iterator<char>()
    );
}

void EmitJson(const json& payload) {
    std::cout << payload.dump() << std::endl;
    std::cout.flush();
}

void EmitResult(const json& data) {
    EmitJson(json{
        {"type", "result"},
        {"data", data}
    });
}

void EmitError(const std::string& message, const std::string& code) {
    EmitJson(json{
        {"type", "error"},
        {"code", code},
        {"message", message}
    });
}

std::string RequireString(const json& input, const char* key) {
    if (!input.contains(key) || !input[key].is_string()) {
        throw std::runtime_error(std::string("Missing or invalid ") + key);
    }
    return input[key].get<std::string>();
}

void RunCorpusPipeline(
    const std::string& model_path,
    const std::string& input_text_path,
    const std::string& output_dir,
    const std::string& semantics_rules_path,
    const teknegram::BuildOptions& build_options,
    const teknegram::ProgressEmitter* progress_emitter
) {
    teknegram::CorpusEngine engine;
    engine.run(model_path, input_text_path, output_dir, semantics_rules_path, build_options, progress_emitter);
}

int RunCliMode(int argc, char** argv) {
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

    teknegram::BuildOptions build_options;
    if (argc > 5) {
        build_options.posting_encoding = teknegram::ParsePostingEncodingMode(argv[5]);
    }
    if (argc > 6) {
        build_options.emit_ngram_positions = std::string(argv[6]) != "false";
    }

    RunCorpusPipeline(model_path, input_text_path, output_dir, semantics_rules_path, build_options, 0);
    std::cout << "Corpus pipeline completed. Output: " << output_dir << "\n";
    return 0;
}

int RunJsonMode(const std::string& input_text) {
    json input_data;
    try {
        input_data = json::parse(input_text);
    } catch (const json::parse_error&) {
        EmitError("Invalid JSON input", "INVALID_JSON");
        std::cerr << "Invalid JSON input\n";
        return 1;
    }

    try {
        const std::string command = RequireString(input_data, "command");
        if (command != "buildCorpus") {
            throw std::runtime_error("Unknown command: " + command);
        }

        const std::string model_path = RequireString(input_data, "modelPath");
        const std::string input_path = RequireString(input_data, "inputPath");
        const std::string output_dir = RequireString(input_data, "outputDir");
        const std::string semantics_rules_path = input_data.contains("semanticsRulesPath")
            && input_data["semanticsRulesPath"].is_string()
            ? input_data["semanticsRulesPath"].get<std::string>()
            : "";
        teknegram::BuildOptions build_options;
        if (input_data.contains("postingFormat") && input_data["postingFormat"].is_string()) {
            build_options.posting_encoding =
                teknegram::ParsePostingEncodingMode(input_data["postingFormat"].get<std::string>());
        }
        if (input_data.contains("emitNgramPositions") && input_data["emitNgramPositions"].is_boolean()) {
            build_options.emit_ngram_positions = input_data["emitNgramPositions"].get<bool>();
        }

        JsonProgressEmitter progress_emitter;
        progress_emitter.emit("Validating build request", 1);

        RunCorpusPipeline(model_path,
                          input_path,
                          output_dir,
                          semantics_rules_path,
                          build_options,
                          &progress_emitter);

        EmitResult(json{
            {"command", command},
            {"outputDir", output_dir},
            {"inputPath", input_path},
            {"modelPath", model_path},
            {"semanticsRulesPath", semantics_rules_path},
            {"postingFormat", teknegram::PostingEncodingModeToString(build_options.posting_encoding)},
            {"emitNgramPositions", build_options.emit_ngram_positions}
        });

        return 0;
    } catch (const std::exception& ex) {
        EmitError(ex.what(), "COMMAND_FAILED");
        std::cerr << ex.what() << "\n";
        return 1;
    }
}

} // namespace

int main(int argc, char** argv) {
    const std::string stdin_text = ReadStdin();

    try {
        if (HasNonWhitespace(stdin_text)) {
            return RunJsonMode(stdin_text);
        }
        return RunCliMode(argc, argv);
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
