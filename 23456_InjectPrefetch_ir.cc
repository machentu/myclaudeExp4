// attr [compute(data_vec, 0x4db68650)] realize_scope = ""
realize data_vec([0, 1], [0, 64], [0, 18], [0, 2], [0, 32]) {
  produce data_vec {
    parallel (C.h.fused, 0, 1152) {
      for (c, 0, 2) {
        for (w, 0, 32) {
          data_vec(0, (C.h.fused/18), (C.h.fused % 18), c, w) =tvm_if_then_else((((((C.h.fused % 18) >= 1) && ((C.h.fused % 18) < 17)) && (w >= 1)) && (w < 31)), placeholder(0, (((C.h.fused/18)*2) + c), ((C.h.fused % 18) - 1), (w - 1)), (int8)0)
        }
      }
    }
  }
  // attr [compute(kernel_vec, 0x4db678d0)] realize_scope = ""
  realize kernel_vec([0, 17], [0, 64], [0, 3], [0, 3], [0, 2], [0, 3]) {
    produce kernel_vec {
      parallel (CO.h.fused, 0, 51) {
        for (CI, 0, 64) {
          for (w, 0, 3) {
            for (ci, 0, 2) {
              vectorized (co, 0, 3) {
                kernel_vec((CO.h.fused/3), CI, (CO.h.fused % 3), w, ci, co) =placeholder((((CO.h.fused/3)*3) + co), ((CI*2) + ci), (CO.h.fused % 3), w)
              }
            }
          }
        }
      }
    }
    // attr [compute(output_unpack, 0x4db6dc90)] realize_scope = ""
    realize output_unpack([0, 1], [0, 51], [0, 16], [0, 30]) {
      produce output_unpack {
        parallel (c.outer.h.fused, 0, 272) {
          // attr [compute(conv, 0x45966860)] realize_scope = ""
          realize conv([0, 1], [(c.outer.h.fused/16), 1], [(c.outer.h.fused % 16), 1], [0, 30], [0, 3]) {
            produce conv {
              // attr [compute(conv.global, 0x4db6b8f0)] realize_scope = "global"
              realize conv.global([0, 1], [(c.outer.h.fused/16), 1], [(c.outer.h.fused % 16), 1], [0, 30], [0, 3]) {
                produce conv.global {
                  unrolled (ow.c.inner.init, 0, 30) {
                    vectorized (oc_block.c.init, 0, 3) {
                      conv.global(0, (c.outer.h.fused/16), (c.outer.h.fused % 16), ow.c.inner.init, oc_block.c.init) =(int64)0
                    }
                  }
                  for (ic.outer, 0, 64) {
                    for (kh, 0, 3) {
                      for (kw, 0, 3) {
                        for (ic.inner, 0, 2) {
                          unrolled (ow.c.inner, 0, 30) {
                            vectorized (oc_block.c, 0, 3) {
                              conv.global(0, (c.outer.h.fused/16), (c.outer.h.fused % 16), ow.c.inner, oc_block.c) =(conv.global(0, (c.outer.h.fused/16), (c.outer.h.fused % 16), ow.c.inner, oc_block.c) + (int64(data_vec(0, ((ic.inner + (ic.outer*2))/2), ((c.outer.h.fused % 16) + kh), ((ic.inner + (ic.outer*2)) % 2), (ow.c.inner + kw)))*int64(kernel_vec((c.outer.h.fused/16), ((ic.inner + (ic.outer*2))/2), kh, kw, ((ic.inner + (ic.outer*2)) % 2), oc_block.c))))
                            }
                          }
                        }
                      }
                    }
                  }
                }
                for (ow.inner, 0, 30) {
                  vectorized (oc_block, 0, 3) {
                    conv(0, (c.outer.h.fused/16), (c.outer.h.fused % 16), ow.inner, oc_block) =conv.global(0, (c.outer.h.fused/16), (c.outer.h.fused % 16), ow.inner, oc_block)
                  }
                }
              }
            }
            for (w.inner, 0, 30) {
              vectorized (c.inner, 0, 3) {
                output_unpack(0, (c.inner + ((c.outer.h.fused/16)*3)), (c.outer.h.fused % 16), w.inner) =conv(0, ((c.inner + ((c.outer.h.fused/16)*3))/3), (c.outer.h.fused % 16), w.inner, ((c.inner + ((c.outer.h.fused/16)*3)) % 3))
              }
            }
          }
        }
      }
    }
  }
}
