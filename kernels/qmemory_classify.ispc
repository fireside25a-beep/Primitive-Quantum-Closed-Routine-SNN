// Classify canonical five-qubit basis codewords without changing a tableau.
// Noncanonical blocks are handled by the existing scalar syndrome decoder.
export void nm_qmemory_classify(const uniform uint32 cells[],
                                uniform int32 count,
                                const uniform uint32 zero[],
                                const uniform uint32 one[],
                                uniform int8 logical[]) {
    foreach (i = 0 ... count) {
        int32 same_zero = 1;
        int32 same_one = 1;
        for (uniform int32 j = 0; j < 30; ++j) {
            uint32 word = cells[i * 30 + j];
            same_zero &= (word == zero[j]);
            same_one &= (word == one[j]);
        }
        logical[i] = same_zero ? 0 : (same_one ? 1 : 2);
    }
}
