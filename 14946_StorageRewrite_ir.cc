// attr [placeholder.global.global] storage_scope = "global"
allocate placeholder.global.global[int8 * 576]
// attr [DepthwiseConv2d.global] storage_scope = "global"
allocate DepthwiseConv2d.global[int64 * 32768]
produce placeholder.global.global {
  parallel (ax0.outer.c, 0, 64) {
    for (ax2.c, 0, 3) {
      for (ax3.c, 0, 3) {
        placeholder.global.global[(((ax0.outer.c*9) + (ax2.c*3)) + ax3.c)] = placeholder[(((ax0.outer.c*9) + (ax2.c*3)) + ax3.c)]
      }
    }
  }
}
produce DepthwiseConv2d.global {
  parallel (c.outer.c, 0, 64) {
    // attr [PaddedInput.global.global] storage_scope = "global"
    allocate PaddedInput.global.global[int8 * 54]
    for (i.c.outer, 0, 32) {
      produce PaddedInput.global.global {
        for (ax2.c, 0, 3) {
          for (ax3.c, 0, 18) {
            PaddedInput.global.global[((ax2.c*18) + ax3.c)] = tvm_if_then_else(((((1 <= (ax2.c + i.c.outer)) && ((ax2.c + i.c.outer) < 33)) && (1 <= ax3.c)) && (ax3.c < 17)), placeholder[(((((c.outer.c*512) + (ax2.c*16)) + (i.c.outer*16)) + ax3.c) - 17)], (int8)0)
          }
        }
      }
      for (j.c.outer, 0, 16) {
        DepthwiseConv2d.global[(((c.outer.c*512) + (i.c.outer*16)) + j.c.outer)] = (int64)0
        for (di, 0, 3) {
          for (dj, 0, 3) {
            DepthwiseConv2d.global[(((c.outer.c*512) + (i.c.outer*16)) + j.c.outer)] = (DepthwiseConv2d.global[(((c.outer.c*512) + (i.c.outer*16)) + j.c.outer)] + (int64(PaddedInput.global.global[(((di*18) + j.c.outer) + dj)])*int64(placeholder.global.global[(((c.outer.c*9) + (di*3)) + dj)])))
          }
        }
      }
    }
  }
}
produce DepthwiseConv2d {
  parallel (c, 0, 64) {
    for (i, 0, 32) {
      for (j, 0, 16) {
        DepthwiseConv2d[(((c*512) + (i*16)) + j)] = DepthwiseConv2d.global[(((c*512) + (i*16)) + j)]
      }
    }
  }
}
