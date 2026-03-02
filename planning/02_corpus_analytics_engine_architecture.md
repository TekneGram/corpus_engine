# Corpus Analytics Engine Architecture

Generated: 2026-03-01

------------------------------------------------------------------------

# 1. Core Token Layer

  ---------------------------------------------------------------------------------------------------------
  type     filename     size     purpose      shape   useful info about useful info about  useful info
                                                      reading queries   writing to binary  about
                                                                                           compression
  -------- ------------ -------- ------------ ------- ----------------- ------------------ ----------------
  tokens   word.bin     uint32   contiguous   0 1 2 3 Used for          Write in single    No delta;
                                 word ids     4 5 2 6 concordance       pass aligned with  optional block
                                              ...     reconstruction;   other token arrays compression
                                                      random access via                    
                                                      mmap                                 

  lemmas   lemma.bin    uint32   lemma id per 4 9 9 2 Used for CQL;      Write alongside    No delta
                                 token        7 1 ... n-grams are built  word.bin           
                                                      from word.bin                          

  pos      pos.bin      uint8    POS per      5 3 3 8 Fast POS          Write during       Extremely
                                 token        2 ...   filtering         parsing            compact

  deprel   deprel.bin   uint8    dependency   1 3 7 2 Dependency        Write during       Extremely
                                 relation id  ...     filtering         parsing            compact

  heads    head.bin     uint32   head token   3 5 5 0 Dependency        Store global token No delta
                                 position     ...     traversal         index              
  ---------------------------------------------------------------------------------------------------------

------------------------------------------------------------------------

# 2. Structural Layer

  --------------------------------------------------------------------------------------------------------
  type        filename              size     purpose     shape       reading     writing     compression
  ----------- --------------------- -------- ----------- ----------- ----------- ----------- -------------
  sentence    sentence_bounds.bin   uint32   sentence    0 15 29 47  Prevent     Write       None
  bounds                                     start       ...         n-gram      during      
                                             positions               crossing    parsing     

  document    doc_ranges.bin        uint32   token       (0,134)     Sampling    Write after None
  ranges                            ×2       start/end   (135,442)   docs and    corpus      
                                             per doc     ...         token       finalized   
                                                                     iteration               

  token→doc   token_doc.bin         uint32   doc id per  1 1 1 2 2   Fast        Write       None
                                             token       ...         filtering   during      
                                                                     by document parsing     
  --------------------------------------------------------------------------------------------------------

------------------------------------------------------------------------

# 3. Search Posting Layer (Token Positions)

Delta + Variable Byte encoding.

  -----------------------------------------------------------------------------------------------------------------
  type      filename          size            purpose     shape             reading       writing     compression
  --------- ----------------- --------------- ----------- ----------------- ------------- ----------- -------------
  lemma     lemma.index.\*    \~1.5           lemma →     lemma →           Used for CQL  Single pass Delta +
  index                       bytes/posting   token       \[12,48,93...\]   and           over        VarByte
                                              positions                     concordance   lemma.bin   

  pos index pos.index.\*      similar         POS → token pos →             POS queries   Single pass Delta +
                                              positions   \[positions\]                               VarByte

  dep       dep.index.\*      similar         head →      head →            Dependency    Build from  Delta +
  reverse                                     child       \[child...\]      traversal     head.bin    VarByte
                                              positions                                               

  2g token  2g.pos.index.\*   small           2-gram →    bundle →          Concordance   Sliding     Delta +
  index                                       start       \[pos...\]                      window      VarByte
                                              positions                                   build       

  3g token  3g.pos.index.\*   moderate        3-gram →    same              Concordance   Sliding     Delta +
  index                                       start                                       window      VarByte
                                              positions                                               

  4g token  4g.pos.index.\*   pruned          4-gram →    same              Concordance   Store freq  Delta +
  index                                       start                                       ≥ 2         VarByte
                                              positions                                               

  5g token  5g.pos.index.\*   pruned          5-gram →    same              Concordance   Store freq  Delta +
  index                                       start                                       ≥ 2         VarByte
                                              positions                                               
  -----------------------------------------------------------------------------------------------------------------

------------------------------------------------------------------------

# 4. Doc-Frequency Layer (Scientific Computing Layer)

Store feature → (doc_id, count) postings.

  -------------------------------------------------------------------------------------------------------
  type      filename           size       purpose   shape             reading     writing   compression
  --------- ------------------ ---------- --------- ----------------- ----------- --------- -------------
  lemma     lemma.docfreq.\*   compact    lemma →   lemma →           Monte Carlo Build     Delta
  docfreq                                 doc       \[(doc,count)\]   summation   from      doc_id +
                                          counts                                  corpus    uint16 count
                                                                                  pass      

  word      word.docfreq.\*    compact    word →    word →            Monte Carlo Build     Delta
  docfreq                                 doc       \[(doc,count)\]   summation   from      doc_id +
                                          counts                                  corpus    uint16 count
                                                                                  pass      

  2g        2g.docfreq.\*      moderate   2-gram →  same              Monte Carlo Sliding   Delta
  docfreq                                 doc                                     window    
                                          counts                                            

  3g        3g.docfreq.\*      larger     3-gram →  same              Monte Carlo Sliding   Delta
  docfreq                                 doc                                     window    
                                          counts                                            

  4g        4g.docfreq.\*      pruned     4-gram →  same              Core Monte  freq ≥ 2  Delta
  docfreq                                 doc                         Carlo       only      
                                          counts                                            

  5g        5g.docfreq.\*      pruned     5-gram →  same              Advanced    freq ≥ 2  Delta
  docfreq                                 doc                         Monte Carlo only      
                                          counts                                            
  -------------------------------------------------------------------------------------------------------

------------------------------------------------------------------------

# 5. N-Gram Lexicon Layer

  --------------------------------------------------------------------------------------------------------------
  type      filename         size      purpose         shape                 reading     writing   compression
  --------- ---------------- --------- --------------- --------------------- ----------- --------- -------------
  2g        2g.lexicon.bin   8 bytes × (l1,l2)         \[(12,5),(5,7)...\]   Maps        Build     Raw uint32
  lexicon                    entries                                         bundle_id → from hash 
                                                                             words       map       

  3g        3g.lexicon.bin   12 bytes  (l1,l2,l3)      same                  Same        Same      Raw uint32
  lexicon                    × entries                                                             

  4g        4g.lexicon.bin   16 bytes  (l1,l2,l3,l4)   same                  Same        freq ≥ 2  Raw uint32
  lexicon                    × entries                                                             

  5g        5g.lexicon.bin   20 bytes  same            same                  Same        freq ≥ 2  Raw uint32
  lexicon                    × entries                                                             
  --------------------------------------------------------------------------------------------------------------

------------------------------------------------------------------------

# 6. Dictionary Layer (Text Reconstruction)

  -------------------------------------------------------------------------------------------------
  type     filename             size    purpose   shape    reading          writing   compression
  -------- -------------------- ------- --------- -------- ---------------- --------- -------------
  word     word.lexicon.bin     small   id →      sorted   Concordance      Build     Front coding
  dict                                  string    string   reconstruction   once      
                                                  pool                                

  lemma    lemma.lexicon.bin    small   id →      same     Stats output     Same      Front coding
  dict                                  string                                        

  pos dict pos.lexicon.bin      tiny    id →      small    Query output     Static    None
                                        string    array                               

  deprel   deprel.lexicon.bin   tiny    id →      small    Dependency       Static    None
  dict                                  string    array    output                     
  -------------------------------------------------------------------------------------------------

------------------------------------------------------------------------

# 7. Metadata Layer (SQLite)

  ------------------------------------------------------------------------
  type             filename                    purpose
  ---------------- --------------------------- ---------------------------
  metadata         corpus.db                   document metadata,
                                               subcorpora, experiment
                                               results

  ------------------------------------------------------------------------

------------------------------------------------------------------------

# Monte Carlo Scientific Computing Design

## Core Principle

Do NOT loop:

for each run: for each feature: scan feature posting list

Instead invert the loop.

## Correct Structure

1.  Precompute a sparse matrix:

    doc → list of (feature_id, count)

2.  For each Monte Carlo run:

    -   Sample document IDs
    -   Initialize accumulator vector
    -   For each selected document:
        -   For each (feature_id, count) in document:
            accumulator\[feature_id\] += count

3.  Store accumulator result.

4.  Repeat.

## Why This Works

-   Each run touches only sampled documents.
-   No scanning of entire corpus per run.
-   No token-level reconstruction.
-   Pure integer addition inside tight loops.
-   Memory locality is preserved.
-   Fully parallelizable across runs.

## Required Data Structure

Sparse matrix format:

doc_feature_offsets.bin\
doc_feature_entries.bin

Where:

doc_feature_offsets\[d\] → (offset, length)\
doc_feature_entries\[offset : offset+length\] → (feature_id, count)

This allows:

O(total features in sampled docs) per run

instead of

O(total corpus features) per run

------------------------------------------------------------------------

# Summary

This architecture combines:

-   Columnar token storage
-   Inverted index search
-   Document-level sparse feature matrix
-   Scientific computing optimized loops
-   SQL for metadata only

It is scalable, cache-friendly, and designed for tens of thousands of
Monte Carlo simulations.
