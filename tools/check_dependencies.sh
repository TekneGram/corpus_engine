#!/usr/bin/env bash
set -euo pipefail

SEARCH_TOOL="rg"
if ! command -v rg >/dev/null 2>&1; then
  SEARCH_TOOL="grep"
fi

search_for_pattern() {
  local pattern="$1"
  local dir="$2"

  if [[ "$SEARCH_TOOL" == "rg" ]]; then
    rg -n -e "$pattern" "$dir" >/dev/null
  else
    grep -R -n -E -- "$pattern" "$dir" >/dev/null 2>&1
  fi
}

# 1) Search layer must NOT include sparse matrix headers.
if search_for_pattern '#include ".*corpus_computing_engine/' corpus_search_layer; then
  echo "Dependency violation: corpus_search_layer includes corpus_computing_engine" >&2
  exit 1
fi

# 2) Sparse matrix layer must NOT include inverted index headers.
if search_for_pattern '#include ".*corpus_search_layer/(inverted_index_builder|dependency_index_builder|ngram_builder)\.hpp"' corpus_computing_engine; then
  echo "Dependency violation: corpus_computing_engine includes search index builders" >&2
  exit 1
fi

# 3) Parse layer must not include representation/search/computing layers.
if search_for_pattern '#include ".*corpus_representation_layer/|#include ".*corpus_search_layer/|#include ".*corpus_computing_engine/' parse_layer; then
  echo "Dependency violation: parse_layer includes forbidden layers" >&2
  exit 1
fi

if [[ "$SEARCH_TOOL" == "grep" ]]; then
  echo "Dependency checks passed. (fallback: grep; install ripgrep for faster checks)"
else
  echo "Dependency checks passed."
fi
