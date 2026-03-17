# 8. Sparse Matrix Layer (Scientific Computing Core)

This layer stores a document-first sparse matrix (CSR-style) optimized
for Monte Carlo simulation and large-scale numerical corpus analytics.

Each feature family (word, lemma, 2gram, 3gram, 4gram) has three binary
files:

-   `.spm.meta.bin`
-   `.spm.offsets.bin`
-   `.spm.entries.bin`

------------------------------------------------------------------------

## 8.1 Sparse Matrix Meta Layer

  ---------------------------------------------------------------------------------------------
  Type    Filename             Size    Purpose      Shape    Reading   Writing    Compression
  ------- -------------------- ------- ------------ -------- --------- ---------- -------------
  Word    word.spm.meta.bin    64      Defines      Fixed    Read once Written    None
  SPM                         bytes   matrix       header   at        after
  meta                                dimensions   struct   startup   build
                                      and                             complete
                                      validation

  Lemma   lemma.spm.meta.bin   64      Defines      Fixed    Read once Written    None
  SPM                          bytes   matrix       header   at        after      
  meta                                 dimensions   struct   startup   build      
                                       and                             complete   
                                       validation                                 

  2g SPM  2gram.spm.meta.bin   64      Same for     Fixed    Same      Same       None
  meta                         bytes   2-grams      header                        
                                                    struct                        

  3g SPM  3gram.spm.meta.bin   64      Same for     Fixed    Same      Same       None
  meta                         bytes   3-grams      header                        
                                                    struct                        

  4g SPM  4gram.spm.meta.bin   64      Same for     Fixed    Same      Same       None
  meta                         bytes   4-grams      header                        
                                                    struct                        
                     
  ---------------------------------------------------------------------------------------------

### Header Structure (64 bytes)

    uint32 magic              // 0x53504D31 ("SPM1")
    uint32 version            // format version (1)
    uint64 num_docs           // number of documents (D)
    uint64 num_features       // number of features (F)
    uint64 num_nonzero        // NNZ (total non-zero doc-feature pairs)
    uint32 entry_size_bytes   // size of each entry (8)
    uint32 reserved           // future use
    padding                   // zero-filled to 64 bytes

------------------------------------------------------------------------

## 8.2 Sparse Matrix Offsets Layer

  ------------------------------------------------------------------------------------------------------------
  Type      Filename                Size      Purpose      Shape          Reading      Writing   Compression
  --------- ----------------------- --------- ------------ -------------- ------------ --------- -------------
  Word SPM  word.spm.offsets.bin    (D+1) ×   doc → row    offsets\[d\] → Sequential   Written   None
  offsets                           uint64    boundaries   index into     access per   during
                                                           entries        document     matrix
                                                                                       build

  Lemma SPM lemma.spm.offsets.bin   (D+1) ×   doc → row    offsets\[d\] → Sequential   Written   None
  offsets                           uint64    boundaries   index into     access per   during    
                                                           entries        document     matrix    
                                                                                       build     

  2g SPM    2gram.spm.offsets.bin   Same      Same         Same           Same         Same      None
  offsets                           pattern                                                      

  3g SPM    3gram.spm.offsets.bin   Same      Same         Same           Same         Same      None
  offsets                                                                                        

  4g SPM    4gram.spm.offsets.bin   Same      Same         Same           Same         Same      None
  offsets                                                                                                                                                    
  ------------------------------------------------------------------------------------------------------------

### Interpretation

Entries for document `d` are located at:

    entries[offsets[d] ... offsets[d+1] - 1]

Properties:

-   offsets\[0\] = 0
-   offsets\[D\] = NNZ
-   Monotonically increasing
-   Enables O(1) row access
-   Memory-mappable without decoding

------------------------------------------------------------------------

## 8.3 Sparse Matrix Entries Layer

  -----------------------------------------------------------------------------------------------------
  Type      Filename                Size    Purpose        Shape      Reading   Writing   Compression
  --------- ----------------------- ------- -------------- ---------- --------- --------- -------------
  Word SPM  word.spm.entries.bin    NNZ × 8 (feature_id,   struct     Tight     Built     None (raw
  entries                           bytes   count) per     {uint32,   Monte     after     binary)
                                            document       uint32}    Carlo     corpus
                                                                      inner     pass
                                                                      loop

  Lemma SPM lemma.spm.entries.bin   NNZ × 8 (feature_id,   struct     Tight     Built     None (raw
  entries                           bytes   count) per     {uint32,   Monte     after     binary)
                                            document       uint32}    Carlo     corpus    
                                                                      inner     pass      
                                                                      loop                

  2g SPM    2gram.spm.entries.bin   Same    Same           Same       Same      Same      None
  entries                                                                                 

  3g SPM    3gram.spm.entries.bin   Same    Same           Same       Same      Same      None
  entries                                                                                 

  4g SPM    4gram.spm.entries.bin   Same    Same           Same       Same      Same      None
  entries                                                                                                                                                             
  -----------------------------------------------------------------------------------------------------

### Entry Structure

    struct {
        uint32 feature_id;
        uint32 count;
    };

Properties:

-   Entries for each document are contiguous
-   Entries may be sorted by feature_id (recommended)
-   No delta encoding
-   No variable byte encoding
-   Optimized for sequential integer addition
-   Fully memory-mappable
-   Designed for scientific computing, not search

The `2gram`, `3gram`, and `4gram` sparse matrices are built from
word-based n-gram bundle IDs.

------------------------------------------------------------------------

## 8.4 Sparse Matrix Access Pattern

Monte Carlo loop structure:

    accumulator[F] = {0}

    for doc in sampled_docs:
        start = offsets[doc]
        end   = offsets[doc+1]

        for i in start .. end-1:
            fid = entries[i].feature_id
            cnt = entries[i].count
            accumulator[fid] += cnt

Time complexity per run:

O(total features in sampled documents)

Not:

O(total features in corpus)

This is why the Sparse Matrix Layer is separate from the inverted index
layer.
