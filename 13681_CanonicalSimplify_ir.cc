// attr [placeholder.global.global] storage_scope = "global"
allocate placeholder.global.global[int8 * 128 * 1 * 3 * 3 * 1]
produce placeholder.global.global {
  parallel (ax0.outer.c, 0, 128) {
    for (ax2.c, 0, 3) {
      for (ax3.c, 0, 3) {
        placeholder.global.global[(((ax0.outer.c*9) + (ax2.c*3)) + ax3.c)] = placeholder[(((ax0.outer.c*9) + (ax2.c*3)) + ax3.c)]
      }
    }
  }
}
// attr [DepthwiseConv2d.global] storage_scope = "global"
allocate DepthwiseConv2d.global[int64 * 1 * 128 * 8 * 4 * 1]
produce DepthwiseConv2d.global {
  parallel (c.outer.c, 0, 128) {
    for (i.c.outer, 0, 8) {
      // attr [PaddedInput.global.global] storage_scope = "global"
      allocate PaddedInput.global.global[int8 * 1 * 1 * 3 * 6 * 1]
      produce PaddedInput.global.global {
        for (ax2.c, 0, 3) {
          for (ax3.c, 0, 6) {
            PaddedInput.global.global[((ax2.c*6) + ax3.c)] = tvm_if_then_else(((((1 <= (ax2.c + i.c.outer)) && ((ax2.c + i.c.outer) < 9)) && (1 <= ax3.c)) && (ax3.c < 5)), placeholder[(((((c.outer.c*32) + (ax2.c*4)) + (i.c.outer*4)) + ax3.c) - 5)], (int8)0)
          }
        }
      }
      for (j.c.outer, 0, 4) {
        DepthwiseConv2d.global[(((c.outer.c*32) + (i.c.outer*4)) + j.c.outer)] = (int64)0
        for (di, 0, 3) {
          for (dj, 0, 3) {
            DepthwiseConv2d.global[(((c.outer.c*32) + (i.c.outer*4)) + j.c.outer)] = (DepthwiseConv2d.global[(((c.outer.c*32) + (i.c.outer*4)) + j.c.outer)] + (int64(PaddedInput.global.global[(((di*6) + j.c.outer) + dj)])*int64(placeholder.global.global[(((c.outer.c*9) + (di*3)) + dj)])))
          }
        }
      }
    }
  }
}
produce DepthwiseConv2d {
  parallel (c, 0, 128) {
    for (i, 0, 8) {
      for (j, 0, 4) {
        DepthwiseConv2d[(((c*32) + (i*4)) + j)] = DepthwiseConv2d.global[(((c*32) + (i*4)) + j)]
      }
    }
  }
}
