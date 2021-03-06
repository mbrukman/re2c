#include <stdlib.h>

#include "src/debug/debug.h"
#include "src/dfa/determinization.h"
#include "src/dfa/tag_history.h"


namespace re2c
{

int32_t precedence(determ_context_t &ctx, const clos_t &x, const clos_t &y
    , int32_t &prec1, int32_t &prec2)
{
    const int32_t idx1 = x.tlook, idx2 = y.tlook;
    const uint32_t orig1 = x.origin, orig2 = y.origin;
    int prec = 0;

    if (idx1 == idx2 && orig1 == orig2) {
        prec1 = prec2 = MAX_RHO;
        return prec;
    }

    const std::vector<Tag> &tags = ctx.dc_dfa.tags;
    tag_history_t &hist = ctx.dc_taghistory;

    const bool fork_frame = orig1 == orig2;
    if (fork_frame) {
        prec1 = prec2 = MAX_RHO;
    }
    else {
        const kernel_t *k = ctx.dc_kernels[ctx.dc_origin];
        prec = unpack_leftmost(k->prectbl[orig1 * k->size + orig2]);
        prec1 = unpack_longest(k->prectbl[orig1 * k->size + orig2]);
        prec2 = unpack_longest(k->prectbl[orig2 * k->size + orig1]);
    }

    tag_info_t info1, info2;
    int32_t i1 = idx1, i2 = idx2;
    for (; i1 != i2; ) {
        if (i1 > i2) {
            info1 = hist.info(i1);
            prec1 = std::min(prec1, tags[info1.idx].height);
            i1 = hist.pred(i1);
        }
        else {
            info2 = hist.info(i2);
            prec2 = std::min(prec2, tags[info2.idx].height);
            i2 = hist.pred(i2);
        }
        DINCCOUNT_CLLENGTH(ctx, 1);
    }
    if (i1 != HROOT) {
        DASSERT(fork_frame);
        const int32_t h = tags[hist.info(i1).idx].height;
        prec1 = std::min(prec1, h);
        prec2 = std::min(prec2, h);
    }
    DINCCOUNT_CLPREC(ctx);

    // longest precedence
    if (prec1 > prec2) return -1;
    if (prec1 < prec2) return  1;

    // leftmost precedence
    if (fork_frame) {
        // equal => not less
        if (i1 == idx1 && i2 == idx2) return 0;

        // shorter => less
        if (i1 == idx1) return -1;
        if (i2 == idx2) return  1;

        const uint32_t tag1 = info1.idx, tag2 = info2.idx;
        const bool neg1 = info1.neg, neg2 = info2.neg;

        // can't be both closing
        DASSERT(!(tag1 % 2 == 1 && tag2 % 2 == 1));

        // closing vs opening: closing wins
        if (tag1 % 2 == 1) return -1;
        if (tag2 % 2 == 1) return  1;

        // can't be both negative
        DASSERT(!(neg1 && neg2));

        // positive vs negative: positive wins
        if (neg1) return  1;
        if (neg2) return -1;

        // positive vs positive: smaller wins
        // (this case is only possible because multiple
        // top-level RE don't have proper negative tags)
        if (tag1 < tag2) return -1;
        if (tag1 > tag2) return  1;

        DASSERT(false); // unreachable
    }
    return prec;
}

int32_t unpack_longest(int32_t packed)
{
    // take lower 30 bits and sign-extend
    return static_cast<int32_t>(static_cast<uint32_t>(packed) << 2u) >> 2u;
}

int32_t unpack_leftmost(int32_t packed)
{
    // take higher 2 bits and sign-extend
    return packed >> 30u;
}

int32_t pack(int32_t longest, int32_t leftmost)
{
    // avoid signed overflows by using unsigned arithmetics
    uint32_t u_longest = static_cast<uint32_t>(longest);
    uint32_t u_leftmost = static_cast<uint32_t>(leftmost);

    // leftmost: higher 2 bits, longest: lower 30 bits
    uint32_t u_packed = (u_longest & 0x3fffFFFF) | (u_leftmost << 30u);
    int32_t packed = static_cast<int32_t>(u_packed);

    DASSERT(unpack_longest(packed) == longest
        && unpack_leftmost(packed) == leftmost);

    return packed;
}

} // namespace re2c
