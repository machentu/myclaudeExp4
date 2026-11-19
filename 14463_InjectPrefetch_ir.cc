// attr [compute(placeholder.global.global, 0x153cafa0)] realize_scope = "global"
realize placeholder.global.global([0, 256], [0, 1], [0, 1], [0, 1], [0, 1]) {
  produce placeholder.global.global {
    parallel (ax0.outer.c, 0, 256) {
      placeholder.global.global(ax0.outer.c, 0, 0, 0, 0) =placeholder((0 + ax0.outer.c), 0, 0, 0)
    }
  }
  // attr [compute(DepthwiseConv2d.global, 0x26e0c370)] realize_scope = "global"
  realize DepthwiseConv2d.global([0, 1], [0, 256], [0, 16], [0, 8], [0, 1]) {
    produce DepthwiseConv2d.global {
      parallel (c.outer.c, 0, 256) {
        for (i.c.outer, 0, 16) {
          // attr [compute(PaddedInput.global.global, 0x153c99c0)] realize_scope = "global"
          realize PaddedInput.global.global([0, 1], [c.outer.c, 1], [i.c.outer, 1], [0, 8], [0, 1]) {
            produce PaddedInput.global.global {
              for (ax3.c, 0, 8) {
                PaddedInput.global.global(0, c.outer.c, i.c.outer, ax3.c, 0) =placeholder(0, (0 + c.outer.c), i.c.outer, ax3.c)
              }
            }
            for (j.c.outer, 0, 8) {
              DepthwiseConv2d.global(0, c.outer.c, i.c.outer, j.c.outer, 0) =(int64)0
              DepthwiseConv2d.global(0, c.outer.c, i.c.outer, j.c.outer, 0) =(DepthwiseConv2d.global(0, c.outer.c, i.c.outer, j.c.outer, 0) + (int64(PaddedInput.global.global(0, (0 + c.outer.c), (i.c.outer + 0), (j.c.outer + 0), 0))*int64(placeholder.global.global((0 + c.outer.c), 0, 0, 0, 0))))
            }
          }
        }
      }
    }
    // attr [compute(DepthwiseConv2d, 0x15287120)] realize_scope = ""
    realize DepthwiseConv2d([0, 1], [0, 256], [0, 16], [0, 8]) {
      produce DepthwiseConv2d {
        parallel (c, 0, 256) {
          for (i, 0, 16) {
            for (j, 0, 8) {
              DepthwiseConv2d(0, c, i, j) =DepthwiseConv2d.global(0, c, i, j, 0)
            }
          }
        }
      }
    }
  }
}
