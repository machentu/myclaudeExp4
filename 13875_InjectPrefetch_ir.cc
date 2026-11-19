// attr [compute(placeholder.global.global, 0x1d15a0a0)] realize_scope = "global"
realize placeholder.global.global([0, 384], [0, 1], [0, 1], [0, 1], [0, 1]) {
  produce placeholder.global.global {
    parallel (ax0.outer.c, 0, 384) {
      placeholder.global.global(ax0.outer.c, 0, 0, 0, 0) =placeholder((0 + ax0.outer.c), 0, 0, 0)
    }
  }
  // attr [compute(DepthwiseConv2d.global, 0x1eebdd20)] realize_scope = "global"
  realize DepthwiseConv2d.global([0, 1], [0, 384], [0, 8], [0, 4], [0, 1]) {
    produce DepthwiseConv2d.global {
      parallel (c.outer.c, 0, 384) {
        for (i.c.outer, 0, 8) {
          // attr [compute(PaddedInput.global.global, 0x2dc98c10)] realize_scope = "global"
          realize PaddedInput.global.global([0, 1], [c.outer.c, 1], [i.c.outer, 1], [0, 4], [0, 1]) {
            produce PaddedInput.global.global {
              for (ax3.c, 0, 4) {
                PaddedInput.global.global(0, c.outer.c, i.c.outer, ax3.c, 0) =placeholder(0, (0 + c.outer.c), i.c.outer, ax3.c)
              }
            }
            for (j.c.outer, 0, 4) {
              DepthwiseConv2d.global(0, c.outer.c, i.c.outer, j.c.outer, 0) =(int64)0
              DepthwiseConv2d.global(0, c.outer.c, i.c.outer, j.c.outer, 0) =(DepthwiseConv2d.global(0, c.outer.c, i.c.outer, j.c.outer, 0) + (int64(PaddedInput.global.global(0, (0 + c.outer.c), (i.c.outer + 0), (j.c.outer + 0), 0))*int64(placeholder.global.global((0 + c.outer.c), 0, 0, 0, 0))))
            }
          }
        }
      }
    }
    // attr [compute(DepthwiseConv2d, 0x1d8b0da0)] realize_scope = ""
    realize DepthwiseConv2d([0, 1], [0, 384], [0, 8], [0, 4]) {
      produce DepthwiseConv2d {
        parallel (c, 0, 384) {
          for (i, 0, 8) {
            for (j, 0, 4) {
              DepthwiseConv2d(0, c, i, j) =DepthwiseConv2d.global(0, c, i, j, 0)
            }
          }
        }
      }
    }
  }
}
