#include "corpus_engine.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <cmath>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../corpus_computing_engine/sparse_matrix_builder.hpp"
#include "../corpus_representation_layer/core_token_layer.hpp"
#include "../corpus_representation_layer/dictionary_builder.hpp"
#include "../corpus_representation_layer/metadata_writer.hpp"
#include "../corpus_representation_layer/structural_layer.hpp"
#include "../corpus_shared_layer/binary_header.hpp"
#include "../corpus_shared_layer/binary_writer.hpp"
#include "../corpus_search_layer/dependency_index_builder.hpp"
#include "../corpus_search_layer/docfreq_builder.hpp"
#include "../corpus_search_layer/inverted_index_builder.hpp"
#include "../corpus_search_layer/ngram_builder.hpp"
#include "../parse_layer/text_file_reader.hpp"
#include "../parse_layer/udpipe_parser.hpp"

namespace teknegram {

    namespace {

        struct InputFile {
            std::string absolute_path;
            std::string relative_path;
        };

        struct SemanticRule {
            enum RuleType {
                kDepth,
                kRegex
            };

            RuleType type;
            std::string key_name;
            std::uint32_t depth;
            std::regex pattern;
            std::uint32_t capture_group;
        };

        bool IsDirectory(const std::string& path) {
            struct stat st;
            return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
        }

        bool IsRegularFile(const std::string& path) {
            struct stat st;
            return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
        }

        void EnsureDir(const std::string& path) {
            struct stat st;
            if (stat(path.c_str(), &st) == 0) {
                if (!S_ISDIR(st.st_mode)) {
                    throw std::runtime_error("Output path exists but is not a directory: " + path);
                }
                return;
            }

            if (mkdir(path.c_str(), 0755) != 0) {
                throw std::runtime_error("Failed to create output directory: " + path);
            }
        }

        std::string BaseName(const std::string& path) {
            const std::string::size_type slash = path.find_last_of('/');
            return (slash == std::string::npos) ? path : path.substr(slash + 1);
        }

        std::string DirName(const std::string& path) {
            const std::string::size_type slash = path.find_last_of('/');
            return (slash == std::string::npos) ? std::string() : path.substr(0, slash);
        }

        std::vector<std::string> SplitPath(const std::string& path) {
            std::vector<std::string> parts;
            if (path.empty()) {
                return parts;
            }

            std::string::size_type start = 0;
            while (start <= path.size()) {
                const std::string::size_type slash = path.find('/', start);
                if (slash == std::string::npos) {
                    const std::string tail = path.substr(start);
                    if (!tail.empty()) {
                        parts.push_back(tail);
                    }
                    break;
                }

                const std::string part = path.substr(start, slash - start);
                if (!part.empty()) {
                    parts.push_back(part);
                }
                start = slash + 1;
            }
            return parts;
        }

        std::vector<std::string> SplitByTab(const std::string& line) {
            std::vector<std::string> parts;
            std::string current;
            for (std::size_t i = 0; i < line.size(); ++i) {
                if (line[i] == '\t') {
                    parts.push_back(current);
                    current.clear();
                } else {
                    current.push_back(line[i]);
                }
            }
            parts.push_back(current);
            return parts;
        }

        void CollectFilesRecursive(const std::string& root_dir,
                                   const std::string& relative_dir,
                                   std::vector<InputFile>* files) {
            const std::string current_dir = relative_dir.empty()
                ? root_dir
                : (root_dir + "/" + relative_dir);

            DIR* dir = opendir(current_dir.c_str());
            if (!dir) {
                throw std::runtime_error(
                    "Failed to open input directory: " + current_dir + " (" + std::strerror(errno) + ")");
            }

            while (const struct dirent* entry = readdir(dir)) {
                const std::string name(entry->d_name);
                if (name == "." || name == "..") {
                    continue;
                }

                const std::string child_relative = relative_dir.empty()
                    ? name
                    : (relative_dir + "/" + name);
                const std::string child_full = root_dir + "/" + child_relative;

                if (IsDirectory(child_full)) {
                    CollectFilesRecursive(root_dir, child_relative, files);
                    continue;
                }

                if (IsRegularFile(child_full)) {
                    InputFile input_file;
                    input_file.absolute_path = child_full;
                    input_file.relative_path = child_relative;
                    files->push_back(input_file);
                }
            }
            closedir(dir);
        }

        std::vector<InputFile> ResolveInputFiles(const std::string& input_path) {
            std::vector<InputFile> files;

            if (IsRegularFile(input_path)) {
                InputFile input_file;
                input_file.absolute_path = input_path;
                input_file.relative_path = BaseName(input_path);
                files.push_back(input_file);
                return files;
            }
            if (!IsDirectory(input_path)) {
                throw std::runtime_error("Input path is neither a regular file nor a directory: " + input_path);
            }

            CollectFilesRecursive(input_path, std::string(), &files);

            std::sort(files.begin(), files.end(),
                      [](const InputFile& lhs, const InputFile& rhs) {
                          return lhs.relative_path < rhs.relative_path;
                      });
            if (files.empty()) {
                throw std::runtime_error("Input directory has no regular files (recursive): " + input_path);
            }
            return files;
        }

        std::vector<SemanticRule> LoadSemanticRules(const std::string& rules_path) {
            std::vector<SemanticRule> rules;
            if (rules_path.empty()) {
                return rules;
            }

            std::ifstream in(rules_path.c_str(), std::ios::in);
            if (!in) {
                throw std::runtime_error("Failed to open semantic rules file: " + rules_path);
            }

            std::string line;
            std::uint32_t line_number = 0;
            while (std::getline(in, line)) {
                ++line_number;
                if (!line.empty() && line[line.size() - 1] == '\r') {
                    line.erase(line.size() - 1);
                }

                if (line.empty() || line[0] == '#') {
                    continue;
                }

                const std::vector<std::string> fields = SplitByTab(line);
                if (fields.size() < 3) {
                    std::ostringstream oss;
                    oss << "Malformed semantic rule at line " << line_number
                        << ": expected at least 3 tab-separated fields.";
                    throw std::runtime_error(oss.str());
                }

                SemanticRule rule;
                rule.key_name = fields[0];
                if (rule.key_name.empty()) {
                    std::ostringstream oss;
                    oss << "Malformed semantic rule at line " << line_number << ": empty key name.";
                    throw std::runtime_error(oss.str());
                }

                if (fields[1] == "depth") {
                    rule.type = SemanticRule::kDepth;
                    try {
                        rule.depth = static_cast<std::uint32_t>(std::stoul(fields[2]));
                    } catch (const std::exception&) {
                        std::ostringstream oss;
                        oss << "Malformed semantic depth rule at line " << line_number
                            << ": depth must be an unsigned integer.";
                        throw std::runtime_error(oss.str());
                    }
                    rule.capture_group = 0;
                    rule.pattern = std::regex();
                } else if (fields[1] == "regex") {
                    if (fields.size() < 4) {
                        std::ostringstream oss;
                        oss << "Malformed semantic regex rule at line " << line_number
                            << ": expected key<TAB>regex<TAB>pattern<TAB>capture_group.";
                        throw std::runtime_error(oss.str());
                    }
                    rule.type = SemanticRule::kRegex;
                    rule.depth = 0;
                    try {
                        rule.pattern = std::regex(fields[2]);
                    } catch (const std::regex_error&) {
                        std::ostringstream oss;
                        oss << "Invalid regex pattern at line " << line_number << ".";
                        throw std::runtime_error(oss.str());
                    }
                    try {
                        rule.capture_group = static_cast<std::uint32_t>(std::stoul(fields[3]));
                    } catch (const std::exception&) {
                        std::ostringstream oss;
                        oss << "Malformed semantic regex rule at line " << line_number
                            << ": capture_group must be an unsigned integer.";
                        throw std::runtime_error(oss.str());
                    }
                } else {
                    std::ostringstream oss;
                    oss << "Unknown semantic rule type at line " << line_number
                        << ": expected 'depth' or 'regex'.";
                    throw std::runtime_error(oss.str());
                }

                rules.push_back(rule);
            }

            return rules;
        }

        void AssignSemanticFromRules(const std::vector<SemanticRule>& rules,
                                     const std::vector<std::string>& path_segments,
                                     const std::string& relative_directory,
                                     std::map<std::string, std::string>* assignments) {
            for (std::size_t i = 0; i < rules.size(); ++i) {
                const SemanticRule& rule = rules[i];
                if (assignments->find(rule.key_name) != assignments->end()) {
                    continue;
                }

                if (rule.type == SemanticRule::kDepth) {
                    if (rule.depth < path_segments.size()) {
                        (*assignments)[rule.key_name] = path_segments[rule.depth];
                    }
                    continue;
                }

                std::smatch match;
                if (!std::regex_search(relative_directory, match, rule.pattern)) {
                    continue;
                }
                if (rule.capture_group >= match.size()) {
                    continue;
                }
                const std::string value = match[rule.capture_group].str();
                if (!value.empty()) {
                    (*assignments)[rule.key_name] = value;
                }
            }
        }

        void WriteSemanticFilterArtifacts(
            const std::string& output_dir,
            const std::uint32_t num_docs,
            const std::map<std::uint32_t, std::string>& key_lexicon,
            const std::map<std::uint32_t, std::pair<std::uint32_t, std::string> >& value_lexicon,
            const std::map<std::uint32_t, std::vector<std::uint32_t> >& value_postings,
            const std::vector<std::vector<std::pair<std::uint32_t, std::uint32_t> > >& doc_groups) {

            BinaryWriter key_lexicon_out(output_dir + "/semantic.key.lexicon.bin");
            const std::uint32_t key_count = static_cast<std::uint32_t>(key_lexicon.size());
            key_lexicon_out.write(key_count);
            for (std::map<std::uint32_t, std::string>::const_iterator it = key_lexicon.begin();
                 it != key_lexicon.end();
                 ++it) {
                key_lexicon_out.write(it->first);
                const std::uint32_t text_len = static_cast<std::uint32_t>(it->second.size());
                key_lexicon_out.write(text_len);
                if (text_len > 0) {
                    key_lexicon_out.write_raw(it->second.data(), text_len);
                }
            }

            BinaryWriter value_lexicon_out(output_dir + "/semantic.value.lexicon.bin");
            const std::uint32_t value_count = static_cast<std::uint32_t>(value_lexicon.size());
            value_lexicon_out.write(value_count);
            for (std::map<std::uint32_t, std::pair<std::uint32_t, std::string> >::const_iterator it =
                    value_lexicon.begin();
                 it != value_lexicon.end();
                 ++it) {
                value_lexicon_out.write(it->first);               // value_id
                value_lexicon_out.write(it->second.first);        // key_id
                const std::uint32_t text_len = static_cast<std::uint32_t>(it->second.second.size());
                value_lexicon_out.write(text_len);
                if (text_len > 0) {
                    value_lexicon_out.write_raw(it->second.second.data(), text_len);
                }
            }

            std::uint32_t max_value_id = 0;
            if (!value_lexicon.empty()) {
                max_value_id = value_lexicon.rbegin()->first;
            }

            std::vector<IndexHeaderEntry> value_header(value_lexicon.empty()
                                                       ? 0
                                                       : (static_cast<std::size_t>(max_value_id) + 1U));
            std::vector<std::uint32_t> value_entries;
            std::uint64_t offset = 0;
            for (std::size_t value_id = 0; value_id < value_header.size(); ++value_id) {
                value_header[value_id].offset = offset;
                const std::map<std::uint32_t, std::vector<std::uint32_t> >::const_iterator posting_it =
                    value_postings.find(static_cast<std::uint32_t>(value_id));
                if (posting_it == value_postings.end()) {
                    value_header[value_id].length = 0;
                    continue;
                }

                const std::vector<std::uint32_t>& docs = posting_it->second;
                value_header[value_id].length = static_cast<std::uint32_t>(docs.size());
                value_entries.insert(value_entries.end(), docs.begin(), docs.end());
                offset += static_cast<std::uint64_t>(docs.size());
            }

            BinaryWriter value_header_out(output_dir + "/semantic.value_doc.header");
            const std::uint32_t header_size = static_cast<std::uint32_t>(value_header.size());
            value_header_out.write(header_size);
            if (!value_header.empty()) {
                value_header_out.write_raw(value_header.data(),
                                           value_header.size() * sizeof(IndexHeaderEntry));
            }

            BinaryWriter value_entries_out(output_dir + "/semantic.value_doc.entries");
            if (!value_entries.empty()) {
                value_entries_out.write_raw(value_entries.data(),
                                            value_entries.size() * sizeof(std::uint32_t));
            }

            std::vector<std::uint64_t> doc_offsets(static_cast<std::size_t>(num_docs) + 1U, 0U);
            std::vector<std::uint32_t> doc_group_entries;
            std::uint64_t running = 0;
            for (std::uint32_t doc_id = 0; doc_id < num_docs; ++doc_id) {
                doc_offsets[doc_id] = running;
                const std::vector<std::pair<std::uint32_t, std::uint32_t> >& groups =
                    doc_groups[static_cast<std::size_t>(doc_id)];
                for (std::size_t i = 0; i < groups.size(); ++i) {
                    doc_group_entries.push_back(groups[i].first);  // key_id
                    doc_group_entries.push_back(groups[i].second); // value_id
                    ++running;
                }
            }
            doc_offsets[num_docs] = running;

            BinaryWriter doc_offsets_out(output_dir + "/semantic.doc_groups.offsets.bin");
            if (!doc_offsets.empty()) {
                doc_offsets_out.write_raw(doc_offsets.data(),
                                          doc_offsets.size() * sizeof(std::uint64_t));
            }

            BinaryWriter doc_entries_out(output_dir + "/semantic.doc_groups.entries.bin");
            if (!doc_group_entries.empty()) {
                doc_entries_out.write_raw(doc_group_entries.data(),
                                          doc_group_entries.size() * sizeof(std::uint32_t));
            }
        }

        int ClampPercent(int percent) {
            if (percent < 0) {
                return 0;
            }
            if (percent > 100) {
                return 100;
            }
            return percent;
        }

        int ComputeFileLoopPercent(std::size_t processed_files, std::size_t total_files) {
            if (total_files == 0U) {
                return 80;
            }

            const double progress = static_cast<double>(processed_files) /
                static_cast<double>(total_files);
            return ClampPercent(5 + static_cast<int>(std::lround(progress * 75.0)));
        }

    } // namespace

    void CorpusEngine::run(const std::string& model_path,
                        const std::string& input_text_path,
                        const std::string& output_dir,
                        const std::string& semantics_rules_path,
                        const BuildOptions& build_options,
                        const ProgressEmitter* progress_emitter) const {
        NullProgressEmitter default_emitter;
        const ProgressEmitter* emitter = progress_emitter ? progress_emitter : &default_emitter;

        emitter->emit("Preparing corpus build", 0);
        EnsureDir(output_dir);

        TextFileReader reader;
        UDPipeParser parser(model_path);
        const std::vector<InputFile> input_files = ResolveInputFiles(input_text_path);
        const std::vector<SemanticRule> semantic_rules = LoadSemanticRules(semantics_rules_path);
        emitter->emit("Discovered " + std::to_string(static_cast<unsigned long long>(input_files.size())) +
                          " input files",
                      5);

        DictionaryBuilder dictionary_builder;
        StructuralLayer structural_layer(output_dir);
        CoreTokenLayer core_token_layer(output_dir);
        MetadataWriter metadata_writer(output_dir + "/corpus.db");
        std::map<std::string, std::uint32_t> segment_to_id;
        std::uint32_t next_segment_id = 0;
        std::map<std::string, std::uint32_t> semantic_key_to_id;
        std::uint32_t next_semantic_key_id = 0;
        std::map<std::pair<std::uint32_t, std::string>, std::uint32_t> semantic_value_to_id;
        std::uint32_t next_semantic_value_id = 0;
        std::map<std::uint32_t, std::string> semantic_key_lexicon;
        std::map<std::uint32_t, std::pair<std::uint32_t, std::string> > semantic_value_lexicon;
        std::map<std::uint32_t, std::vector<std::uint32_t> > semantic_value_postings;
        std::vector<std::vector<std::pair<std::uint32_t, std::uint32_t> > > doc_groups(input_files.size());

        for (std::size_t i = 0; i < input_files.size(); ++i) {
            const std::uint32_t document_id = static_cast<std::uint32_t>(i);
            const std::string document_name = BaseName(input_files[i].relative_path);
            const int current_percent = ComputeFileLoopPercent(i, input_files.size());

            emitter->emit("Reading " + document_name, current_percent);
            const std::string text = reader.read_file(input_files[i].absolute_path);
            emitter->emit("Parsing " + document_name, current_percent);
            const ParsedDocument doc = parser.parse(text, document_id);
            emitter->emit("Writing " + document_name, current_percent);
            core_token_layer.append_document(doc, &dictionary_builder, &structural_layer);

            metadata_writer.upsert_document(document_id,
                                            input_files[i].absolute_path,
                                            "unknown",
                                            input_files[i].relative_path);

            const std::string directory = DirName(input_files[i].relative_path);
            const std::vector<std::string> path_segments = SplitPath(directory);
            for (std::size_t depth = 0; depth < path_segments.size(); ++depth) {
                const std::map<std::string, std::uint32_t>::const_iterator found =
                    segment_to_id.find(path_segments[depth]);

                std::uint32_t segment_id = 0;
                if (found == segment_to_id.end()) {
                    segment_id = next_segment_id++;
                    segment_to_id[path_segments[depth]] = segment_id;
                    metadata_writer.upsert_folder_segment(segment_id, path_segments[depth]);
                } else {
                    segment_id = found->second;
                }

                metadata_writer.upsert_document_segment(document_id,
                                                       static_cast<std::uint32_t>(depth),
                                                       segment_id);
            }

            std::map<std::string, std::string> semantic_assignments;
            AssignSemanticFromRules(semantic_rules,
                                    path_segments,
                                    directory,
                                    &semantic_assignments);

            for (std::map<std::string, std::string>::const_iterator it = semantic_assignments.begin();
                 it != semantic_assignments.end();
                 ++it) {
                std::uint32_t key_id = 0;
                const std::map<std::string, std::uint32_t>::const_iterator key_found =
                    semantic_key_to_id.find(it->first);
                if (key_found == semantic_key_to_id.end()) {
                    key_id = next_semantic_key_id++;
                    semantic_key_to_id[it->first] = key_id;
                    semantic_key_lexicon[key_id] = it->first;
                    metadata_writer.upsert_semantic_key(key_id, it->first);
                } else {
                    key_id = key_found->second;
                }

                const std::pair<std::uint32_t, std::string> value_lookup(key_id, it->second);
                std::uint32_t value_id = 0;
                const std::map<std::pair<std::uint32_t, std::string>, std::uint32_t>::const_iterator value_found =
                    semantic_value_to_id.find(value_lookup);
                if (value_found == semantic_value_to_id.end()) {
                    value_id = next_semantic_value_id++;
                    semantic_value_to_id[value_lookup] = value_id;
                    semantic_value_lexicon[value_id] = std::make_pair(key_id, it->second);
                    metadata_writer.upsert_semantic_value(value_id, key_id, it->second);
                } else {
                    value_id = value_found->second;
                }

                metadata_writer.upsert_document_group(document_id, key_id, value_id);
                semantic_value_postings[value_id].push_back(document_id);
                doc_groups[document_id].push_back(std::make_pair(key_id, value_id));
            }

            emitter->emit("Processed " + document_name,
                          ComputeFileLoopPercent(i + 1U, input_files.size()));
        }

        emitter->emit("Building semantic filter artifacts", 81);
        WriteSemanticFilterArtifacts(output_dir,
                                     static_cast<std::uint32_t>(input_files.size()),
                                     semantic_key_lexicon,
                                     semantic_value_lexicon,
                                     semantic_value_postings,
                                     doc_groups);

        emitter->emit("Writing lexicons", 82);
        dictionary_builder.write_lexicons(output_dir, build_options.posting_encoding);

        InvertedIndexBuilder inverted_builder;
        emitter->emit("Building word index", 84);
        inverted_builder.build_word_index(output_dir, build_options.posting_encoding);
        emitter->emit("Building lemma index", 85);
        inverted_builder.build_lemma_index(output_dir, build_options.posting_encoding);
        emitter->emit("Building POS index", 86);
        inverted_builder.build_pos_index(output_dir, build_options.posting_encoding);

        DependencyIndexBuilder dep_builder;
        emitter->emit("Building dependency index", 89);
        dep_builder.build(output_dir, build_options.posting_encoding);

        NGramBuilder<2> ngram2;
        NGramBuilder<3> ngram3;
        NGramBuilder<4> ngram4;
        emitter->emit("Building 2-grams", 92);
        const FeatureRowsResult ngram2_rows = ngram2.build(output_dir, build_options);
        emitter->emit("Building 3-grams", 93);
        const FeatureRowsResult ngram3_rows = ngram3.build(output_dir, build_options);
        emitter->emit("Building 4-grams", 94);
        const FeatureRowsResult ngram4_rows = ngram4.build(output_dir, build_options);

        DocFreqBuilder docfreq_builder;
        emitter->emit("Building word doc frequencies", 95);
        const FeatureRowsResult word_rows =
            docfreq_builder.build_word_docfreq(output_dir, build_options.posting_encoding);
        emitter->emit("Building lemma doc frequencies", 96);
        const FeatureRowsResult lemma_rows =
            docfreq_builder.build_lemma_docfreq(output_dir, build_options.posting_encoding);

        SparseMatrixBuilder sparse_builder;
        emitter->emit("Building sparse matrices", 99);
        sparse_builder.build_matrix(output_dir, "word", word_rows);
        sparse_builder.build_matrix(output_dir, "lemma", lemma_rows);
        sparse_builder.build_matrix(output_dir, "2gram", ngram2_rows);
        sparse_builder.build_matrix(output_dir, "3gram", ngram3_rows);
        sparse_builder.build_matrix(output_dir, "4gram", ngram4_rows);
        emitter->emit("Corpus build complete", 100);
    }

} // namespace teknegram
