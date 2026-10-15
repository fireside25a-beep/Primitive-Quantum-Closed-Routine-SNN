// Independent codeword checks on a pretransposed row-word plane.
// This candidate measures whether contiguous lane loads repay the transpose.
export void nm_qmemory_classify_soa(const uniform uint32 planes[],
                                    uniform int32 count,
                                    const uniform uint32 zero[],
                                    const uniform uint32 one[],
                                    uniform int8 logical[]) {
    foreach (i = 0 ... count) {
        int32 same_zero = 1;
        int32 same_one = 1;
        for (uniform int32 j = 0; j < 30; ++j) {
            uint32 word = planes[j * count + i];
            same_zero &= (word == zero[j]);
            same_one &= (word == one[j]);
        }
        logical[i] = same_zero ? 0 : (same_one ? 1 : 2);
    }
}
