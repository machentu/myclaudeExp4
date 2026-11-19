// attr [placeholder.global.global] storage_scope = "global"
allocate placeholder.global.global[int8 * 384]
// attr [DepthwiseConv2d.global] storage_scope = "global"
allocate DepthwiseConv2d.global[int64 * 12288]
produce placeholder.global.global {
  parallel (ax0.outer.c, 0, 384) {
    placeholder.global.global[ax0.outer.c] = placeholder[ax0.outer.c]
  }
}
produce DepthwiseConv2d.global {
  parallel (c.outer.c, 0, 384) {
    // attr [PaddedInput.global.global] storage_scope = "global"
    allocate PaddedInput.global.global[int8 * 4]
    for (i.c.outer, 0, 8) {
      produce PaddedInput.global.global {
        for (ax3.c, 0, 4) {
          PaddedInput.global.global[ax3.c] = placeholder[(((c.outer.c*32) + (i.c.outer*4)) + ax3.c)]
        }
      }
      for (j.c.outer, 0, 4) {
        DepthwiseConv2d.global[(((c.outer.c*32) + (i.c.outer*4)) + j.c.outer)] = (int64)0
        DepthwiseConv2d.global[(((c.outer.c*32) + (i.c.outer*4)) + j.c.outer)] = (DepthwiseConv2d.global[(((c.outer.c*32) + (i.c.outer*4)) + j.c.outer)] + (int64(PaddedInput.global.global[j.c.outer])*int64(placeholder.global.global[c.outer.c])))
      }
    }
  }
}
produce DepthwiseConv2d {
  parallel (c, 0, 384) {
    for (i, 0, 8) {
      for (j, 0, 4) {
        DepthwiseConv2d[(((c*32) + (i*4)) + j)] = DepthwiseConv2d.global[(((c*32) + (i*4)) + j)]
      }
    }
  }
}
