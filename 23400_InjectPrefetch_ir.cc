// attr [compute(data_vec, 0x47ee6c10)] realize_scope = ""
realize data_vec([0, 1], [0, 17], [0, 16], [0, 3], [0, 30]) {
  produce data_vec {
    parallel (C.h.fused, 0, 272) {
      for (c, 0, 3) {
        for (w, 0, 30) {
          data_vec(0, (C.h.fused/16), (C.h.fused % 16), c, w) =placeholder(0, (((C.h.fused/16)*3) + c), (C.h.fused % 16), w)
        }
      }
    }
  }
  // attr [compute(kernel_vec, 0x4786b9a0)] realize_scope = ""
  realize kernel_vec([0, 17], [0, 17], [0, 1], [0, 1], [0, 3], [0, 3]) {
    produce kernel_vec {
      parallel (CO.h.fused, 0, 17) {
        for (CI, 0, 17) {
          for (ci, 0, 3) {
            vectorized (co, 0, 3) {
              kernel_vec(CO.h.fused, CI, 0, 0, ci, co) =placeholder(((CO.h.fused*3) + co), ((CI*3) + ci), 0, 0)
            }
          }
        }
      }
    }
    // attr [compute(output_unpack, 0x4786e9f0)] realize_scope = ""
    realize output_unpack([0, 1], [0, 51], [0, 16], [0, 30]) {
      produce output_unpack {
        parallel (c.outer.h.outer.fused, 0, 272) {
          // attr [compute(conv, 0x4786d890)] realize_scope = ""
          realize conv([0, 1], [(c.outer.h.outer.fused/16), 1], [(c.outer.h.outer.fused % 16), 1], [0, 30], [0, 3]) {
            produce conv {
              // attr [compute(conv.global, 0x480bdaf0)] realize_scope = "global"
              realize conv.global([0, 1], [(c.outer.h.outer.fused/16), 1], [(c.outer.h.outer.fused % 16), 1], [0, 30], [0, 3]) {
                produce conv.global {
                  unrolled (ow.c.inner.init, 0, 30) {
                    vectorized (oc_block.c.init, 0, 3) {
                      conv.global(0, (c.outer.h.outer.fused/16), (c.outer.h.outer.fused % 16), ow.c.inner.init, oc_block.c.init) =(int64)0
                    }
                  }
                  for (ic.outer, 0, 17) {
                    for (ic.inner, 0, 3) {
                      unrolled (ow.c.inner, 0, 30) {
                        vectorized (oc_block.c, 0, 3) {
                          conv.global(0, (c.outer.h.outer.fused/16), (c.outer.h.outer.fused % 16), ow.c.inner, oc_block.c) =(conv.global(0, (c.outer.h.outer.fused/16), (c.outer.h.outer.fused % 16), ow.c.inner, oc_block.c) + (int64(data_vec(0, ((ic.inner + (ic.outer*3))/3), ((c.outer.h.outer.fused % 16) + 0), ((ic.inner + (ic.outer*3)) % 3), (ow.c.inner + 0)))*int64(kernel_vec((c.outer.h.outer.fused/16), ((ic.inner + (ic.outer*3))/3), 0, 0, ((ic.inner + (ic.outer*3)) % 3), oc_block.c))))
                        }
                      }
                    }
                  }
                }
                for (ow, 0, 30) {
                  vectorized (oc_block, 0, 3) {
                    conv(0, (c.outer.h.outer.fused/16), (c.outer.h.outer.fused % 16), ow, oc_block) =conv.global(0, (c.outer.h.outer.fused/16), (c.outer.h.outer.fused % 16), ow, oc_block)
                  }
                }
              }
            }
            for (w.inner, 0, 30) {
              vectorized (c.inner, 0, 3) {
                output_unpack(0, (c.inner + ((c.outer.h.outer.fused/16)*3)), (c.outer.h.outer.fused % 16), w.inner) =conv(0, ((c.inner + ((c.outer.h.outer.fused/16)*3))/3), (c.outer.h.outer.fused % 16), w.inner, ((c.inner + ((c.outer.h.outer.fused/16)*3)) % 3))
              }
            }
          }
        }
      }
    }
  }
}
