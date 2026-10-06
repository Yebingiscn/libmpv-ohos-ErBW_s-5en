#include <assert.h>
#include <stdio.h>
#include "maleoon_policy.h"

int main(void)
{
    assert(!pl_maleoon_name(NULL));
    assert(!pl_maleoon_name("Huawei Mali-G78"));
    assert(!pl_maleoon_name(""));
    assert(!pl_maleoon_name("Maleoo"));
    assert(pl_maleoon_name("MALOON") == false);
    assert(pl_maleoon_name("Huawei Maleoon 910"));
    assert(pl_maleoon_name("maleoon 920"));
    assert(pl_maleoon_name("MALEOON"));
    for (int rx = 1; rx <= 4; rx *= 2) for (int ry = 1; ry <= 2; ry++) {
        int bw = 12 * rx, bh = 16 * ry, tw = 12, th = 16;
        assert(!pl_maleoon_workgroup(false, true, 256, 256, 256, &bw, &bh, &tw, &th));
        assert(!pl_maleoon_workgroup(true, false, 256, 256, 256, &bw, &bh, &tw, &th));
        assert(!pl_maleoon_workgroup(true, true, 32, 256, 256, &bw, &bh, &tw, &th));
        assert(!pl_maleoon_workgroup(true, true, 256, 4, 256, &bw, &bh, &tw, &th));
        assert(bw == 12 * rx && bh == 16 * ry && tw == 12 && th == 16);
        assert(pl_maleoon_workgroup(true, true, 64, 8, 8, &bw, &bh, &tw, &th));
        assert(bw == 8 * rx && bh == 8 * ry && tw == 8 && th == 8);
        // Edge tiles and odd dimensions: every in-bounds output pixel must
        // still have exactly one writer, including 2x upscaling passes.
        int counts[37][61] = {{0}};
        for (int gy = 0; gy < (37 + bh - 1) / bh; gy++)
            for (int gx = 0; gx < (61 + bw - 1) / bw; gx++)
                for (int y = 0; y < th; y++) for (int x = 0; x < tw; x++)
                    for (int dy = 0; dy < ry; dy++) for (int dx = 0; dx < rx; dx++) {
                        int px = gx * bw + x * rx + dx, py = gy * bh + y * ry + dy;
                        if (px < 61 && py < 37) counts[py][px]++;
                    }
        for (int y = 0; y < 37; y++) for (int x = 0; x < 61; x++) assert(counts[y][x] == 1);
    }
    int bw = 24, bh = 32, tw = 16, th = 16;
    assert(!pl_maleoon_workgroup(true, true, 256, 256, 256, &bw, &bh, &tw, &th));
    puts("Maleoon policy and output coverage passed");
}
