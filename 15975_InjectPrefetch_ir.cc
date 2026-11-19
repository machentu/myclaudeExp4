// attr [compute(data_vec, 0x2da1dd80)] realize_scope = ""
realize data_vec([0, 1], [0, 48], [0, 8], [0, 8], [0, 4]) {
  produce data_vec {
    parallel (C.h.fused, 0, 384) {
      for (c, 0, 8) {
        for (w, 0, 4) {
          data_vec(0, (C.h.fused/8), (C.h.fused % 8), c, w) =placeholder(0, (((C.h.fused/8)*8) + c), (C.h.fused % 8), w)
        }
      }
    }
  }
  // attr [compute(kernel_vec, 0x2da1eb50)] realize_scope = ""
  realize kernel_vec([0, 64], [0, 48], [0, 1], [0, 1], [0, 8], [0, 8]) {
    produce kernel_vec {
      parallel (CO.h.fused, 0, 64) {
        for (CI, 0, 48) {
          for (ci, 0, 8) {
            vectorized (co, 0, 8) {
              kernel_vec(CO.h.fused, CI, 0, 0, ci, co) =placeholder(((CO.h.fused*8) + co), ((CI*8) + ci), 0, 0)
            }
          }
        }
      }
    }
    // attr [compute(output_unpack, 0x2da21530)] realize_scope = ""
    realize output_unpack([0, 1], [0, 512], [0, 8], [0, 4]) {
      produce output_unpack {
        parallel (c.outer.h.outer.fused, 0, 128) {
          // attr [compute(conv, 0x2da203d0)] realize_scope = ""
          realize conv([0, 1], [(c.outer.h.outer.fused/2), 1], [((c.outer.h.outer.fused % 2)*4), 4], [0, 4], [0, 8]) {
            produce conv {
              // attr [compute(conv.global, 0x2e3500d0)] realize_scope = "global"
              realize conv.global([0, 1], [(c.outer.h.outer.fused/2), 1], [((c.outer.h.outer.fused % 2)*4), 4], [0, 4], [0, 8]) {
                produce conv.global {
                  unrolled (oh.c.inner.init, 0, 4) {
                    unrolled (ow.c.inner.init, 0, 4) {
                      vectorized (oc_block.c.init, 0, 8) {
                        conv.global(0, (c.outer.h.outer.fused/2), (oh.c.inner.init + ((c.outer.h.outer.fused % 2)*4)), ow.c.inner.init, oc_block.c.init) =(int64)0
                      }
                    }
                  }
                  for (ic.outer, 0, 48) {
                    for (ic.inner, 0, 8) {
                      unrolled (oh.c.inner, 0, 4) {
                        unrolled (ow.c.inner, 0, 4) {
                          vectorized (oc_block.c, 0, 8) {
                            conv.global(0, (c.outer.h.outer.fused/2), (oh.c.inner + ((c.outer.h.outer.fused % 2)*4)), ow.c.inner, oc_block.c) =(conv.global(0, (c.outer.h.outer.fused/2), (oh.c.inner + ((c.outer.h.outer.fused % 2)*4)), ow.c.inner, oc_block.c) + (int64(data_vec(0, ((ic.inner + (ic.outer*8))/8), ((oh.c.inner + ((c.outer.h.outer.fused % 2)*4)) + 0), ((ic.inner + (ic.outer*8)) % 8), (ow.c.inner + 0)))*int64(kernel_vec((c.outer.h.outer.fused/2), ((ic.inner + (ic.outer*8))/8), 0, 0, ((ic.inner + (ic.outer*8)) % 8), oc_block.c))))
                          }
                        }
                      }
                    }
                  }
                }
                for (oh.inner, 0, 4) {
                  for (ow, 0, 4) {
                    vectorized (oc_block, 0, 8) {
                      conv(0, (c.outer.h.outer.fused/2), (oh.inner + ((c.outer.h.outer.fused % 2)*4)), ow, oc_block) =conv.global(0, (c.outer.h.outer.fused/2), (oh.inner + ((c.outer.h.outer.fused % 2)*4)), ow, oc_block)
                    }
                  }
                }
              }
            }
            for (h.inner, 0, 4) {
              for (w.inner, 0, 4) {
                vectorized (c.inner, 0, 8) {
                  output_unpack(0, (c.inner + ((c.outer.h.outer.fused/2)*8)), (h.inner + ((c.outer.h.outer.fused % 2)*4)), w.inner) =conv(0, ((c.inner + ((c.outer.h.outer.fused/2)*8))/8), (h.inner + ((c.outer.h.outer.fused % 2)*4)), w.inner, ((c.inner + ((c.outer.h.outer.fused/2)*8)) % 8))
                }
              }
            }
          }
        }
      }
    }
  }
}
