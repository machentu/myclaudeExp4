// attr [compute(data_vec, 0x2199fd00)] realize_scope = ""
realize data_vec([0, 1], [0, 1], [0, 1], [0, 4], [0, 1]) {
  produce data_vec {
    for (c, 0, 4) {
      data_vec(0, 0, 0, c, 0) =placeholder(0, ((0*4) + c), 0, 0)
    }
  }
  // attr [compute(kernel_vec, 0x2e936c30)] realize_scope = ""
  realize kernel_vec([0, 8], [0, 1], [0, 1], [0, 1], [0, 4], [0, 8]) {
    produce kernel_vec {
      parallel (CO.h.fused, 0, 8) {
        for (ci, 0, 4) {
          vectorized (co, 0, 8) {
            kernel_vec(CO.h.fused, 0, 0, 0, ci, co) =placeholder(((CO.h.fused*8) + co), ((0*4) + ci), 0, 0)
          }
        }
      }
    }
    // attr [compute(output_unpack, 0x311c7400)] realize_scope = ""
    realize output_unpack([0, 1], [0, 64], [0, 1], [0, 1]) {
      produce output_unpack {
        parallel (c.outer.h.outer.fused, 0, 8) {
          // attr [compute(conv, 0x311c98d0)] realize_scope = ""
          realize conv([0, 1], [c.outer.h.outer.fused, 1], [0, 1], [0, 1], [0, 8]) {
            produce conv {
              // attr [compute(conv.global, 0x2e233f70)] realize_scope = "global"
              realize conv.global([0, 1], [c.outer.h.outer.fused, 1], [0, 1], [0, 1], [0, 8]) {
                produce conv.global {
                  vectorized (oc_block.c.init, 0, 8) {
                    conv.global(0, c.outer.h.outer.fused, 0, 0, oc_block.c.init) =(int64)0
                  }
                  for (ic.inner, 0, 4) {
                    vectorized (oc_block.c, 0, 8) {
                      conv.global(0, c.outer.h.outer.fused, 0, 0, oc_block.c) =(conv.global(0, c.outer.h.outer.fused, 0, 0, oc_block.c) + (int64(data_vec(0, (ic.inner/4), (0 + 0), (ic.inner % 4), (0 + 0)))*int64(kernel_vec(c.outer.h.outer.fused, (ic.inner/4), 0, 0, (ic.inner % 4), oc_block.c))))
                    }
                  }
                }
                vectorized (oc_block, 0, 8) {
                  conv(0, c.outer.h.outer.fused, 0, 0, oc_block) =conv.global(0, c.outer.h.outer.fused, 0, 0, oc_block)
                }
              }
            }
            vectorized (c.inner, 0, 8) {
              output_unpack(0, (c.inner + (c.outer.h.outer.fused*8)), 0, 0) =conv(0, ((c.inner + (c.outer.h.outer.fused*8))/8), 0, 0, ((c.inner + (c.outer.h.outer.fused*8)) % 8))
            }
          }
        }
      }
    }
  }
}
