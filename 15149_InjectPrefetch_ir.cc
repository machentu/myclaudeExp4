// attr [compute(data_vec, 0x2dfd2460)] realize_scope = ""
realize data_vec([0, 1], [0, 1], [0, 133], [0, 3], [0, 69]) {
  produce data_vec {
    parallel (C.h.fused, 0, 133) {
      for (c, 0, 3) {
        for (w, 0, 69) {
          data_vec(0, (C.h.fused/133), (C.h.fused % 133), c, w) =tvm_if_then_else((((((C.h.fused % 133) >= 3) && ((C.h.fused % 133) < 131)) && (w >= 3)) && (w < 67)), placeholder(0, (((C.h.fused/133)*3) + c), ((C.h.fused % 133) - 3), (w - 3)), (uint8)0)
        }
      }
    }
  }
  // attr [compute(kernel_vec, 0x2dfd31d0)] realize_scope = ""
  realize kernel_vec([0, 8], [0, 1], [0, 7], [0, 7], [0, 3], [0, 8]) {
    produce kernel_vec {
      parallel (CO.h.fused, 0, 56) {
        for (w, 0, 7) {
          for (ci, 0, 3) {
            vectorized (co, 0, 8) {
              kernel_vec((CO.h.fused/7), 0, (CO.h.fused % 7), w, ci, co) =placeholder((((CO.h.fused/7)*8) + co), ((0*3) + ci), (CO.h.fused % 7), w)
            }
          }
        }
      }
    }
    // attr [compute(output_unpack, 0x1c104e20)] realize_scope = ""
    realize output_unpack([0, 1], [0, 64], [0, 64], [0, 32]) {
      produce output_unpack {
        parallel (c.outer.h.fused, 0, 512) {
          // attr [compute(conv, 0x1c103d80)] realize_scope = ""
          realize conv([0, 1], [(c.outer.h.fused/64), 1], [(c.outer.h.fused % 64), 1], [0, 32], [0, 8]) {
            produce conv {
              for (ow.outer, 0, 2) {
                // attr [compute(conv.global, 0x1b669de0)] realize_scope = "global"
                realize conv.global([0, 1], [(c.outer.h.fused/64), 1], [(c.outer.h.fused % 64), 1], [(ow.outer*16), 16], [0, 8]) {
                  produce conv.global {
                    unrolled (ow.c.inner.init, 0, 16) {
                      vectorized (oc_block.c.init, 0, 8) {
                        conv.global(0, (c.outer.h.fused/64), (c.outer.h.fused % 64), (ow.c.inner.init + (ow.outer*16)), oc_block.c.init) =(int64)0
                      }
                    }
                    for (kh, 0, 7) {
                      for (kw, 0, 7) {
                        for (ic.inner, 0, 3) {
                          unrolled (ow.c.inner, 0, 16) {
                            vectorized (oc_block.c, 0, 8) {
                              conv.global(0, (c.outer.h.fused/64), (c.outer.h.fused % 64), (ow.c.inner + (ow.outer*16)), oc_block.c) =(conv.global(0, (c.outer.h.fused/64), (c.outer.h.fused % 64), (ow.c.inner + (ow.outer*16)), oc_block.c) + (int64(data_vec(0, (ic.inner/3), (((c.outer.h.fused % 64)*2) + kh), (ic.inner % 3), (((ow.c.inner + (ow.outer*16))*2) + kw)))*int64(kernel_vec((c.outer.h.fused/64), (ic.inner/3), kh, kw, (ic.inner % 3), oc_block.c))))
                            }
                          }
                        }
                      }
                    }
                  }
                  for (ow.inner, 0, 16) {
                    vectorized (oc_block, 0, 8) {
                      conv(0, (c.outer.h.fused/64), (c.outer.h.fused % 64), (ow.inner + (ow.outer*16)), oc_block) =conv.global(0, (c.outer.h.fused/64), (c.outer.h.fused % 64), (ow.inner + (ow.outer*16)), oc_block)
                    }
                  }
                }
              }
            }
            for (w.outer, 0, 2) {
              for (w.inner, 0, 16) {
                vectorized (c.inner, 0, 8) {
                  output_unpack(0, (c.inner + ((c.outer.h.fused/64)*8)), (c.outer.h.fused % 64), (w.inner + (w.outer*16))) =conv(0, ((c.inner + ((c.outer.h.fused/64)*8))/8), (c.outer.h.fused % 64), (w.inner + (w.outer*16)), ((c.inner + ((c.outer.h.fused/64)*8)) % 8))
                }
              }
            }
          }
        }
      }
    }
  }
}
