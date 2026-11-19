// attr [compute(placeholder.global.global, 0x2e5bbe40)] realize_scope = "global"
realize placeholder.global.global([0, 96], [0, 1], [0, 3], [0, 3], [0, 1]) {
  produce placeholder.global.global {
    parallel (ax0.outer.c, 0, 96) {
      for (ax2.c, 0, 3) {
        for (ax3.c, 0, 3) {
          placeholder.global.global(ax0.outer.c, 0, ax2.c, ax3.c, 0) =placeholder((0 + ax0.outer.c), 0, ax2.c, ax3.c)
        }
      }
    }
  }
  // attr [compute(DepthwiseConv2d.global, 0x2aec8060)] realize_scope = "global"
  realize DepthwiseConv2d.global([0, 1], [0, 96], [0, 16], [0, 8], [0, 1]) {
    produce DepthwiseConv2d.global {
      parallel (c.outer.c, 0, 96) {
        for (i.c.outer, 0, 16) {
          // attr [compute(PaddedInput.global.global, 0x2d897cd0)] realize_scope = "global"
          realize PaddedInput.global.global([0, 1], [c.outer.c, 1], [i.c.outer, 3], [0, 10], [0, 1]) {
            produce PaddedInput.global.global {
              for (ax2.c, 0, 3) {
                for (ax3.c, 0, 10) {
                  PaddedInput.global.global(0, c.outer.c, (ax2.c + i.c.outer), ax3.c, 0) =tvm_if_then_else((((((ax2.c + i.c.outer) >= 1) && ((ax2.c + i.c.outer) < 17)) && (ax3.c >= 1)) && (ax3.c < 9)), placeholder(0, (0 + c.outer.c), ((ax2.c + i.c.outer) - 1), (ax3.c - 1)), (int8)0)
                }
              }
            }
            for (j.c.outer, 0, 8) {
              DepthwiseConv2d.global(0, c.outer.c, i.c.outer, j.c.outer, 0) =(int64)0
              for (di, 0, 3) {
                for (dj, 0, 3) {
                  DepthwiseConv2d.global(0, c.outer.c, i.c.outer, j.c.outer, 0) =(DepthwiseConv2d.global(0, c.outer.c, i.c.outer, j.c.outer, 0) + (int64(PaddedInput.global.global(0, (0 + c.outer.c), (i.c.outer + di), (j.c.outer + dj), 0))*int64(placeholder.global.global((0 + c.outer.c), 0, di, dj, 0))))
                }
              }
            }
          }
        }
      }
    }
    // attr [compute(DepthwiseConv2d, 0x1c99fb50)] realize_scope = ""
    realize DepthwiseConv2d([0, 1], [0, 96], [0, 16], [0, 8]) {
      produce DepthwiseConv2d {
        parallel (c, 0, 96) {
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
