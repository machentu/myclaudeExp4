// attr [compute(placeholder.global.global, 0x1e3cbd70)] realize_scope = "global"
realize placeholder.global.global([0, 512], [0, 1], [0, 1], [0, 1], [0, 1]) {
  produce placeholder.global.global {
    parallel (ax0.outer.c, 0, 512) {
      placeholder.global.global(ax0.outer.c, 0, 0, 0, 0) =placeholder((0 + ax0.outer.c), 0, 0, 0)
    }
  }
  // attr [compute(DepthwiseConv2d.global, 0x2e492e10)] realize_scope = "global"
  realize DepthwiseConv2d.global([0, 1], [0, 512], [0, 1], [0, 1], [0, 1]) {
    produce DepthwiseConv2d.global {
      parallel (c.outer.c, 0, 512) {
        // attr [compute(PaddedInput.global.global, 0x1c3cdbd0)] realize_scope = "global"
        realize PaddedInput.global.global([0, 1], [c.outer.c, 1], [0, 1], [0, 1], [0, 1]) {
          produce PaddedInput.global.global {
            PaddedInput.global.global(0, c.outer.c, 0, 0, 0) =placeholder(0, (0 + c.outer.c), 0, 0)
          }
          DepthwiseConv2d.global(0, c.outer.c, 0, 0, 0) =(int64)0
          DepthwiseConv2d.global(0, c.outer.c, 0, 0, 0) =(DepthwiseConv2d.global(0, c.outer.c, 0, 0, 0) + (int64(PaddedInput.global.global(0, (0 + c.outer.c), (0 + 0), (0 + 0), 0))*int64(placeholder.global.global((0 + c.outer.c), 0, 0, 0, 0))))
        }
      }
    }
    // attr [compute(DepthwiseConv2d, 0x2dc40370)] realize_scope = ""
    realize DepthwiseConv2d([0, 1], [0, 512], [0, 1], [0, 1]) {
      produce DepthwiseConv2d {
        parallel (c, 0, 512) {
          DepthwiseConv2d(0, c, 0, 0) =DepthwiseConv2d.global(0, c, 0, 0, 0)
        }
      }
    }
  }
}
