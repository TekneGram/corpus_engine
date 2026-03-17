#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if [[ ! -f "udpipe/src/libudpipe.a" ]]; then
  echo "Building UDPipe static library..."
  make -C udpipe/src lib MODE=release
fi

g++ -std=c++11 -Wall -Wextra -pedantic \
  -Iudpipe/src \
  main.cpp \
  parse_layer/text_file_reader.cpp \
  parse_layer/udpipe_parser.cpp \
  corpus_representation_layer/core_token_layer.cpp \
  corpus_representation_layer/dictionary_builder.cpp \
  corpus_representation_layer/structural_layer.cpp \
  corpus_representation_layer/metadata_writer.cpp \
  corpus_search_layer/inverted_index_builder.cpp \
  corpus_search_layer/dependency_index_builder.cpp \
  corpus_search_layer/docfreq_builder.cpp \
  corpus_computing_engine/sparse_matrix_builder.cpp \
  orchestration_layer/progress_emitter.cpp \
  orchestration_layer/corpus_engine.cpp \
  udpipe/src/libudpipe.a -lsqlite3 \
  -o corpus_build_pipeline

echo "Build passed."
