// attr [compute(data_vec, 0x378fa6d0)] realize_scope = ""
realize data_vec([0, 1], [0, 32], [0, 10], [0, 8], [0, 17]) {
  produce data_vec {
    parallel (C.h.fused, 0, 320) {
      for (c, 0, 8) {
        for (w, 0, 17) {
          data_vec(0, (C.h.fused/10), (C.h.fused % 10), c, w) =tvm_if_then_else((((((C.h.fused % 10) >= 1) && ((C.h.fused % 10) < 9)) && (w >= 1)) && (w < 16)), placeholder(0, (((C.h.fused/10)*8) + c), ((C.h.fused % 10) - 1), (w - 1)), (int8)0)
        }
      }
    }
  }
  // attr [compute(kernel_vec, 0x36c16fa0)] realize_scope = ""
  realize kernel_vec([0, 4], [0, 32], [0, 3], [0, 3], [0, 8], [0, 8]) {
    produce kernel_vec {
      parallel (CO.h.fused, 0, 12) {
        for (CI, 0, 32) {
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
    // attr [compute(output_unpack, 0x36c84770)] realize_scope = ""
    realize output_unpack([0, 1], [0, 32], [0, 8], [0, 15]) {
      produce output_unpack {
        parallel (c.outer.h.fused, 0, 32) {
          // attr [compute(conv, 0x3900e7f0)] realize_scope = ""
          realize conv([0, 1], [(c.outer.h.fused/8), 1], [(c.outer.h.fused % 8), 1], [0, 15], [0, 8]) {
            produce conv {
              // attr [compute(conv.global, 0x36c87500)] realize_scope = "global"
              realize conv.global([0, 1], [(c.outer.h.fused/8), 1], [(c.outer.h.fused % 8), 1], [0, 15], [0, 8]) {
                produce conv.global {
                  unrolled (ow.c.inner.init, 0, 15) {
                    vectorized (oc_block.c.init, 0, 8) {
                      conv.global(0, (c.outer.h.fused/8), (c.outer.h.fused % 8), ow.c.inner.init, oc_block.c.init) =(int64)0
                    }
                  }
                  for (ic.outer, 0, 32) {
                    for (kh, 0, 3) {
                      for (kw, 0, 3) {
                        for (ic.inner, 0, 8) {
                          unrolled (ow.c.inner, 0, 15) {
                            vectorized (oc_block.c, 0, 8) {
                              conv.global(0, (c.outer.h.fused/8), (c.outer.h.fused % 8), ow.c.inner, oc_block.c) =(conv.global(0, (c.outer.h.fused/8), (c.outer.h.fused % 8), ow.c.inner, oc_block.c) + (int64(data_vec(0, ((ic.inner + (ic.outer*8))/8), ((c.outer.h.fused % 8) + kh), ((ic.inner + (ic.outer*8)) % 8), (ow.c.inner + kw)))*int64(kernel_vec((c.outer.h.fused/8), ((ic.inner + (ic.outer*8))/8), kh, kw, ((ic.inner + (ic.outer*8)) % 8), oc_block.c))))
                            }
                          }
                        }
                      }
                    }
                  }
                }
                for (ow.inner, 0, 15) {
                  vectorized (oc_block, 0, 8) {
                    conv(0, (c.outer.h.fused/8), (c.outer.h.fused % 8), ow.inner, oc_block) =conv.global(0, (c.outer.h.fused/8), (c.outer.h.fused % 8), ow.inner, oc_block)
                  }
                }
              }
            }
            for (w.inner, 0, 15) {
              vectorized (c.inner, 0, 8) {
                output_unpack(0, (c.inner + ((c.outer.h.fused/8)*8)), (c.outer.h.fused % 8), w.inner) =conv(0, ((c.inner + ((c.outer.h.fused/8)*8))/8), (c.outer.h.fused % 8), w.inner, ((c.inner + ((c.outer.h.fused/8)*8)) % 8))
              }
            }
          }
        }
      }
    }
  }
}
