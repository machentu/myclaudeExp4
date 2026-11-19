// attr [compute(data_vec, 0x1c0169c0)] realize_scope = ""
realize data_vec([0, 1], [0, 16], [0, 1], [0, 8], [0, 1]) {
  produce data_vec {
    parallel (C.h.fused, 0, 16) {
      for (c, 0, 8) {
        data_vec(0, C.h.fused, 0, c, 0) =placeholder(0, ((C.h.fused*8) + c), 0, 0)
      }
    }
  }
  // attr [compute(kernel_vec, 0x1c018320)] realize_scope = ""
  realize kernel_vec([0, 1], [0, 16], [0, 1], [0, 1], [0, 8], [0, 8]) {
    produce kernel_vec {
      for (CI, 0, 16) {
        for (ci, 0, 8) {
          vectorized (co, 0, 8) {
            kernel_vec(0, CI, 0, 0, ci, co) =placeholder(((0*8) + co), ((CI*8) + ci), 0, 0)
          }
        }
      }
    }
    // attr [compute(output_unpack, 0x1d183020)] realize_scope = ""
    realize output_unpack([0, 1], [0, 8], [0, 1], [0, 1]) {
      produce output_unpack {
        // attr [compute(conv, 0x1c019bf0)] realize_scope = ""
        realize conv([0, 1], [0, 1], [0, 1], [0, 1], [0, 8]) {
          produce conv {
            // attr [compute(conv.global, 0x2e64a950)] realize_scope = "global"
            realize conv.global([0, 1], [0, 1], [0, 1], [0, 1], [0, 8]) {
              produce conv.global {
                vectorized (oc_block.c.init, 0, 8) {
                  conv.global(0, 0, 0, 0, oc_block.c.init) =(int64)0
                }
                for (ic.outer, 0, 16) {
                  for (ic.inner, 0, 8) {
                    vectorized (oc_block.c, 0, 8) {
                      conv.global(0, 0, 0, 0, oc_block.c) =(conv.global(0, 0, 0, 0, oc_block.c) + (int64(data_vec(0, ((ic.inner + (ic.outer*8))/8), (0 + 0), ((ic.inner + (ic.outer*8)) % 8), (0 + 0)))*int64(kernel_vec(0, ((ic.inner + (ic.outer*8))/8), 0, 0, ((ic.inner + (ic.outer*8)) % 8), oc_block.c))))
                    }
                  }
                }
              }
              vectorized (oc_block, 0, 8) {
                conv(0, 0, 0, 0, oc_block) =conv.global(0, 0, 0, 0, oc_block)
              }
            }
          }
          vectorized (c.inner, 0, 8) {
            output_unpack(0, c.inner, 0, 0) =conv(0, (c.inner/8), 0, 0, (c.inner % 8))
          }
        }
      }
    }
  }
}
