#include "udpipe_parser.hpp"

#include <cstddef>
#include <map>
#include <stdexcept>

#include "../udpipe/src/model/model.h"
#include "../udpipe/src/sentence/input_format.h"
#include "../udpipe/src/sentence/sentence.h"

namespace teknegram {

    namespace {

        PosId LookupPosId(const std::string& upos) {
            static const std::map<std::string, PosId> kPosMap = {
                {"ADJ", 1}, {"ADP", 2}, {"ADV", 3}, {"AUX", 4}, {"CCONJ", 5},
                {"DET", 6}, {"INTJ", 7}, {"NOUN", 8}, {"NUM", 9}, {"PART", 10},
                {"PRON", 11}, {"PROPN", 12}, {"PUNCT", 13}, {"SCONJ", 14}, {"SYM", 15},
                {"VERB", 16}, {"X", 17}
            };

            std::map<std::string, PosId>::const_iterator it = kPosMap.find(upos);
            if (it == kPosMap.end()) {
                return 0;
            }
            return it->second;
        }

        DeprelId LookupDeprelId(const std::string& deprel) {
            static const std::map<std::string, DeprelId> kDeprelMap = {
                {"acl", 1}, {"advcl", 2}, {"advmod", 3}, {"amod", 4}, {"appos", 5},
                {"aux", 6}, {"case", 7}, {"cc", 8}, {"ccomp", 9}, {"clf", 10},
                {"compound", 11}, {"conj", 12}, {"cop", 13}, {"csubj", 14}, {"dep", 15},
                {"det", 16}, {"discourse", 17}, {"dislocated", 18}, {"expl", 19}, {"fixed", 20},
                {"flat", 21}, {"goeswith", 22}, {"iobj", 23}, {"list", 24}, {"mark", 25},
                {"nmod", 26}, {"nsubj", 27}, {"nummod", 28}, {"obj", 29}, {"obl", 30},
                {"orphan", 31}, {"parataxis", 32}, {"punct", 33}, {"reparandum", 34}, {"root", 35},
                {"vocative", 36}, {"xcomp", 37}
            };

            std::map<std::string, DeprelId>::const_iterator it = kDeprelMap.find(deprel);
            if (it == kDeprelMap.end()) {
                return 0;
            }
            return it->second;
        }

    } // namespace

    UDPipeParser::UDPipeParser(const std::string& model_path) {
        model_.reset(ufal::udpipe::model::load(model_path.c_str()));
        if (!model_) {
            throw std::runtime_error("Failed to load UDPipe model: " + model_path);
        }
    }

    UDPipeParser::~UDPipeParser() {}

    PosId UDPipeParser::to_pos_id(const std::string& upos) const {
        return LookupPosId(upos);
    }

    DeprelId UDPipeParser::to_deprel_id(const std::string& deprel) const {
        return LookupDeprelId(deprel);
    }

    ParsedDocument UDPipeParser::parse(const std::string& text, DocId document_id) const {
        ParsedDocument document;
        document.document_id = document_id;

        std::unique_ptr<ufal::udpipe::input_format> tokenizer(
            model_->new_tokenizer(ufal::udpipe::model::DEFAULT)
        );

        if (!tokenizer) {
            throw std::runtime_error("Model does not support tokenization.");
        }

        tokenizer->set_text(text.c_str());

        ufal::udpipe::sentence sentence;
        std::string error;

        while (tokenizer->next_sentence(sentence, error)) {
            const std::uint32_t sentence_start = static_cast<std::uint32_t>(document.tokens.size());
            document.sentence_starts.push_back(sentence_start);

            if (!model_->tag(sentence, ufal::udpipe::model::DEFAULT, error)) {
                throw std::runtime_error("UDPipe tag failed: " + error);
            }
            if (!model_->parse(sentence, ufal::udpipe::model::DEFAULT, error)) {
                throw std::runtime_error("UDPipe parse failed: " + error);
            }

            for (std::size_t i = 1; i < sentence.words.size(); ++i) {
                const ufal::udpipe::word& w = sentence.words[i];

                ParsedToken token;
                token.word = w.form;
                token.lemma = w.lemma;
                token.pos_id = to_pos_id(w.upostag);
                token.head = static_cast<std::uint32_t>(w.head);
                token.deprel_id = to_deprel_id(w.deprel);

                document.tokens.push_back(token);
            }

            sentence.clear();
        }

        if (!error.empty()) {
            throw std::runtime_error("UDPipe tokenization failed: " + error);
        }

        return document;
    }

} // namespace corpus
