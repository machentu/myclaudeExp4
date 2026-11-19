// attr [placeholder.global.global] storage_scope = "global"
allocate placeholder.global.global[int8 * 512 * 1 * 1 * 1 * 1]
produce placeholder.global.global {
  parallel (ax0.outer.c, 0, 512) {
    placeholder.global.global[ax0.outer.c] = placeholder[ax0.outer.c]
  }
}
// attr [DepthwiseConv2d.global] storage_scope = "global"
allocate DepthwiseConv2d.global[int64 * 1 * 512 * 1 * 1 * 1]
produce DepthwiseConv2d.global {
  parallel (c.outer.c, 0, 512) {
    // attr [PaddedInput.global.global] storage_scope = "global"
    allocate PaddedInput.global.global[int8 * 1 * 1 * 1 * 1 * 1]
    produce PaddedInput.global.global {
      PaddedInput.global.global[0] = placeholder[c.outer.c]
    }
    DepthwiseConv2d.global[c.outer.c] = (int64)0
    DepthwiseConv2d.global[c.outer.c] = (DepthwiseConv2d.global[c.outer.c] + (int64(PaddedInput.global.global[0])*int64(placeholder.global.global[c.outer.c])))
  }
}
produce DepthwiseConv2d {
  parallel (c, 0, 512) {
    DepthwiseConv2d[c] = DepthwiseConv2d.global[c]
  }
}
