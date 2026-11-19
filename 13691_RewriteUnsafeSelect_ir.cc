// attr [placeholder.global.global] storage_scope = "global"
allocate placeholder.global.global[int8 * 1152]
// attr [DepthwiseConv2d.global] storage_scope = "global"
allocate DepthwiseConv2d.global[int64 * 4096]
produce placeholder.global.global {
  parallel (ax0.outer.c, 0, 128) {
    for (ax2.c, 0, 3) {
      for (ax3.c, 0, 3) {
        placeholder.global.global[((((ax0.outer.c*3) + ax2.c)*3) + ax3.c)] = placeholder[((((ax0.outer.c*3) + ax2.c)*3) + ax3.c)]
      }
    }
  }
}
produce DepthwiseConv2d.global {
  parallel (c.outer.c, 0, 128) {
    // attr [PaddedInput.global.global] storage_scope = "global"
    allocate PaddedInput.global.global[int8 * 18]
    for (i.c.outer, 0, 8) {
      produce PaddedInput.global.global {
        for (ax2.c, 0, 3) {
          for (ax3.c, 0, 6) {
            PaddedInput.global.global[((ax2.c*6) + ax3.c)] = tvm_if_then_else((((((1 - i.c.outer) <= ax2.c) && (ax2.c < (9 - i.c.outer))) && (1 <= ax3.c)) && (ax3.c < 5)), placeholder[((((((c.outer.c*8) + ax2.c) + i.c.outer)*4) + ax3.c) + -5)], (int8)0)
          }
        }
      }
      for (j.c.outer, 0, 4) {
        DepthwiseConv2d.global[((((c.outer.c*8) + i.c.outer)*4) + j.c.outer)] = (int64)0
        for (di, 0, 3) {
          for (dj, 0, 3) {
            DepthwiseConv2d.global[((((c.outer.c*8) + i.c.outer)*4) + j.c.outer)] = (DepthwiseConv2d.global[((((c.outer.c*8) + i.c.outer)*4) + j.c.outer)] + (int64(PaddedInput.global.global[(((di*6) + j.c.outer) + dj)])*int64(placeholder.global.global[((((c.outer.c*3) + di)*3) + dj)])))
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
        DepthwiseConv2d[((((c*8) + i)*4) + j)] = DepthwiseConv2d.global[((((c*8) + i)*4) + j)]
      }
    }
  }
}
