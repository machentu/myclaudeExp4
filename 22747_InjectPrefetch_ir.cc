// attr [compute(data_vec, 0x36f5a4c0)] realize_scope = ""
realize data_vec([0, 1], [0, 4], [0, 18], [0, 8], [0, 32]) {
  produce data_vec {
    parallel (C.h.fused, 0, 72) {
      for (c, 0, 8) {
        for (w, 0, 32) {
          data_vec(0, (C.h.fused/18), (C.h.fused % 18), c, w) =tvm_if_then_else((((((C.h.fused % 18) >= 1) && ((C.h.fused % 18) < 17)) && (w >= 1)) && (w < 31)), placeholder(0, (((C.h.fused/18)*8) + c), ((C.h.fused % 18) - 1), (w - 1)), (int8)0)
        }
      }
    }
  }
  // attr [compute(kernel_vec, 0x369981a0)] realize_scope = ""
  realize kernel_vec([0, 4], [0, 4], [0, 3], [0, 3], [0, 8], [0, 8]) {
    produce kernel_vec {
      parallel (CO.h.fused, 0, 12) {
        for (CI, 0, 4) {
          for (w, 0, 3) {
            for (ci, 0, 8) {
              vectorized (co, 0, 8) {
                kernel_vec((CO.h.fused/3), CI, (CO.h.fused % 3), w, ci, co) =placeholder((((CO.h.fused/3)*8) + co), ((CI*8) + ci), (CO.h.fused % 3), w)
              }
            }
          }
        }
      }
    }
    // attr [compute(output_unpack, 0x386f7650)] realize_scope = ""
    realize output_unpack([0, 1], [0, 32], [0, 16], [0, 30]) {
      produce output_unpack {
        parallel (c.outer.h.fused, 0, 64) {
          // attr [compute(conv, 0x3699e340)] realize_scope = ""
          realize conv([0, 1], [(c.outer.h.fused/16), 1], [(c.outer.h.fused % 16), 1], [0, 30], [0, 8]) {
            produce conv {
              // attr [compute(conv.global, 0x31073750)] realize_scope = "global"
              realize conv.global([0, 1], [(c.outer.h.fused/16), 1], [(c.outer.h.fused % 16), 1], [0, 30], [0, 8]) {
                produce conv.global {
                  unrolled (ow.c.inner.init, 0, 30) {
                    vectorized (oc_block.c.init, 0, 8) {
                      conv.global(0, (c.outer.h.fused/16), (c.outer.h.fused % 16), ow.c.inner.init, oc_block.c.init) =(int64)0
                    }
                  }
                  for (ic.outer, 0, 4) {
                    for (kh, 0, 3) {
                      for (kw, 0, 3) {
                        for (ic.inner, 0, 8) {
                          unrolled (ow.c.inner, 0, 30) {
                            vectorized (oc_block.c, 0, 8) {
                              conv.global(0, (c.outer.h.fused/16), (c.outer.h.fused % 16), ow.c.inner, oc_block.c) =(conv.global(0, (c.outer.h.fused/16), (c.outer.h.fused % 16), ow.c.inner, oc_block.c) + (int64(data_vec(0, ((ic.inner + (ic.outer*8))/8), ((c.outer.h.fused % 16) + kh), ((ic.inner + (ic.outer*8)) % 8), (ow.c.inner + kw)))*int64(kernel_vec((c.outer.h.fused/16), ((ic.inner + (ic.outer*8))/8), kh, kw, ((ic.inner + (ic.outer*8)) % 8), oc_block.c))))
                            }
                          }
                        }
                      }
                    }
                  }
                }
                for (ow.inner, 0, 30) {
                  vectorized (oc_block, 0, 8) {
                    conv(0, (c.outer.h.fused/16), (c.outer.h.fused % 16), ow.inner, oc_block) =conv.global(0, (c.outer.h.fused/16), (c.outer.h.fused % 16), ow.inner, oc_block)
                  }
                }
              }
            }
            for (w.inner, 0, 30) {
              vectorized (c.inner, 0, 8) {
                output_unpack(0, (c.inner + ((c.outer.h.fused/16)*8)), (c.outer.h.fused % 16), w.inner) =conv(0, ((c.inner + ((c.outer.h.fused/16)*8))/8), (c.outer.h.fused % 16), w.inner, ((c.inner + ((c.outer.h.fused/16)*8)) % 8))
              }
            }
          }
        }
      }
    }
  }
}
