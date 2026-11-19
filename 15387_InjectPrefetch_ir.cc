// attr [compute(data_vec, 0x1b039aa0)] realize_scope = ""
realize data_vec([0, 1], [0, 16], [0, 1], [0, 4], [0, 1]) {
  produce data_vec {
    parallel (C.h.fused, 0, 16) {
      for (c, 0, 4) {
        data_vec(0, C.h.fused, 0, c, 0) =placeholder(0, ((C.h.fused*4) + c), 0, 0)
      }
    }
  }
  // attr [compute(kernel_vec, 0x2d796c30)] realize_scope = ""
  realize kernel_vec([0, 1], [0, 16], [0, 1], [0, 1], [0, 4], [0, 4]) {
    produce kernel_vec {
      for (CI, 0, 16) {
        for (ci, 0, 4) {
          vectorized (co, 0, 4) {
            kernel_vec(0, CI, 0, 0, ci, co) =placeholder(((0*4) + co), ((CI*4) + ci), 0, 0)
          }
        }
      }
    }
    // attr [compute(output_unpack, 0x1c98a7c0)] realize_scope = ""
    realize output_unpack([0, 1], [0, 4], [0, 1], [0, 1]) {
      produce output_unpack {
        // attr [compute(conv, 0x227f53f0)] realize_scope = ""
        realize conv([0, 1], [0, 1], [0, 1], [0, 1], [0, 4]) {
          produce conv {
            // attr [compute(conv.global, 0x1c98d2e0)] realize_scope = "global"
            realize conv.global([0, 1], [0, 1], [0, 1], [0, 1], [0, 4]) {
              produce conv.global {
                vectorized (oc_block.c.init, 0, 4) {
                  conv.global(0, 0, 0, 0, oc_block.c.init) =(int64)0
                }
                for (ic.outer, 0, 16) {
                  for (ic.inner, 0, 4) {
                    vectorized (oc_block.c, 0, 4) {
                      conv.global(0, 0, 0, 0, oc_block.c) =(conv.global(0, 0, 0, 0, oc_block.c) + (int64(data_vec(0, ((ic.inner + (ic.outer*4))/4), (0 + 0), ((ic.inner + (ic.outer*4)) % 4), (0 + 0)))*int64(kernel_vec(0, ((ic.inner + (ic.outer*4))/4), 0, 0, ((ic.inner + (ic.outer*4)) % 4), oc_block.c))))
                    }
                  }
                }
              }
              vectorized (oc_block, 0, 4) {
                conv(0, 0, 0, 0, oc_block) =conv.global(0, 0, 0, 0, oc_block)
              }
            }
          }
          vectorized (c.inner, 0, 4) {
            output_unpack(0, c.inner, 0, 0) =conv(0, (c.inner/4), 0, 0, (c.inner % 4))
          }
        }
      }
    }
  }
}
