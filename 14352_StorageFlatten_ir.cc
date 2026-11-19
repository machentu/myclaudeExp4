// attr [placeholder.global.global] storage_scope = "global"
allocate placeholder.global.global[int8 * 96 * 1 * 3 * 3 * 1]
produce placeholder.global.global {
  parallel (ax0.outer.c, 0, 96) {
    for (ax2.c, 0, 3) {
      for (ax3.c, 0, 3) {
        placeholder.global.global[((((ax0.outer.c*3) + ax2.c)*3) + ax3.c)] = placeholder[((((ax0.outer.c*3) + ax2.c)*3) + ax3.c)]
      }
    }
  }
}
// attr [DepthwiseConv2d.global] storage_scope = "global"
allocate DepthwiseConv2d.global[int64 * 1 * 96 * 16 * 8 * 1]
produce DepthwiseConv2d.global {
  parallel (c.outer.c, 0, 96) {
    for (i.c.outer, 0, 16) {
      // attr [PaddedInput.global.global] storage_scope = "global"
      allocate PaddedInput.global.global[int8 * 1 * 1 * 3 * 10 * 1]
      produce PaddedInput.global.global {
        for (ax2.c, 0, 3) {
          for (ax3.c, 0, 10) {
            PaddedInput.global.global[((ax2.c*10) + ax3.c)] = tvm_if_then_else((((((ax2.c + i.c.outer) >= 1) && ((ax2.c + i.c.outer) < 17)) && (ax3.c >= 1)) && (ax3.c < 9)), placeholder[(((((c.outer.c*16) + (ax2.c + i.c.outer))*8) + ax3.c) + -9)], (int8)0)
          }
        }
      }
      for (j.c.outer, 0, 8) {
        DepthwiseConv2d.global[((((c.outer.c*16) + i.c.outer)*8) + j.c.outer)] = (int64)0
        for (di, 0, 3) {
          for (dj, 0, 3) {
            DepthwiseConv2d.global[((((c.outer.c*16) + i.c.outer)*8) + j.c.outer)] = (DepthwiseConv2d.global[((((c.outer.c*16) + i.c.outer)*8) + j.c.outer)] + (int64(PaddedInput.global.global[((di*10) + (j.c.outer + dj))])*int64(placeholder.global.global[((((c.outer.c*3) + di)*3) + dj)])))
          }
        }
      }
    }
  }
}
produce DepthwiseConv2d {
  parallel (c, 0, 96) {
    for (i, 0, 16) {
      for (j, 0, 8) {
        DepthwiseConv2d[((((c*16) + i)*8) + j)] = DepthwiseConv2d.global[((((c*16) + i)*8) + j)]
      }
    }
  }
}
