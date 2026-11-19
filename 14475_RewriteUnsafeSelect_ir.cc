// attr [placeholder.global.global] storage_scope = "global"
allocate placeholder.global.global[int8 * 256]
// attr [DepthwiseConv2d.global] storage_scope = "global"
allocate DepthwiseConv2d.global[int64 * 32768]
produce placeholder.global.global {
  parallel (ax0.outer.c, 0, 256) {
    placeholder.global.global[ax0.outer.c] = placeholder[ax0.outer.c]
  }
}
produce DepthwiseConv2d.global {
  parallel (c.outer.c, 0, 256) {
    // attr [PaddedInput.global.global] storage_scope = "global"
    allocate PaddedInput.global.global[int8 * 8]
    for (i.c.outer, 0, 16) {
      produce PaddedInput.global.global {
        for (ax3.c, 0, 8) {
          PaddedInput.global.global[ax3.c] = placeholder[((((c.outer.c*16) + i.c.outer)*8) + ax3.c)]
        }
      }
      for (j.c.outer, 0, 8) {
        DepthwiseConv2d.global[((((c.outer.c*16) + i.c.outer)*8) + j.c.outer)] = (int64)0
        DepthwiseConv2d.global[((((c.outer.c*16) + i.c.outer)*8) + j.c.outer)] = (DepthwiseConv2d.global[((((c.outer.c*16) + i.c.outer)*8) + j.c.outer)] + (int64(PaddedInput.global.global[j.c.outer])*int64(placeholder.global.global[c.outer.c])))
      }
    }
  }
}
produce DepthwiseConv2d {
  parallel (c, 0, 256) {
    for (i, 0, 16) {
      for (j, 0, 8) {
        DepthwiseConv2d[((((c*16) + i)*8) + j)] = DepthwiseConv2d.global[((((c*16) + i)*8) + j)]
      }
    }
  }
}
