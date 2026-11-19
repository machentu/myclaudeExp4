// attr [compute(data_vec, 0x4746b6c0)] realize_scope = ""
realize data_vec([0, 1], [0, 17], [0, 8], [0, 3], [0, 15]) {
  produce data_vec {
    parallel (C.h.fused, 0, 136) {
      for (c, 0, 3) {
        for (w, 0, 15) {
          data_vec(0, (C.h.fused/8), (C.h.fused % 8), c, w) =placeholder(0, (((C.h.fused/8)*3) + c), (C.h.fused % 8), w)
        }
      }
    }
  }
  // attr [compute(kernel_vec, 0x484ea850)] realize_scope = ""
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
    // attr [compute(output_unpack, 0x43a8e000)] realize_scope = ""
    realize output_unpack([0, 1], [0, 51], [0, 8], [0, 15]) {
      produce output_unpack {
        parallel (c.outer.h.outer.fused, 0, 68) {
          // attr [compute(conv, 0x484e5870)] realize_scope = ""
          realize conv([0, 1], [(c.outer.h.outer.fused/4), 1], [((c.outer.h.outer.fused % 4)*2), 2], [0, 15], [0, 3]) {
            produce conv {
              // attr [compute(conv.global, 0x43a90d60)] realize_scope = "global"
              realize conv.global([0, 1], [(c.outer.h.outer.fused/4), 1], [((c.outer.h.outer.fused % 4)*2), 2], [0, 15], [0, 3]) {
                produce conv.global {
                  unrolled (oh.c.inner.init, 0, 2) {
                    unrolled (ow.c.inner.init, 0, 15) {
                      vectorized (oc_block.c.init, 0, 3) {
                        conv.global(0, (c.outer.h.outer.fused/4), (oh.c.inner.init + ((c.outer.h.outer.fused % 4)*2)), ow.c.inner.init, oc_block.c.init) =(int64)0
                      }
                    }
                  }
                  for (ic.outer, 0, 17) {
                    for (ic.inner, 0, 3) {
                      unrolled (oh.c.inner, 0, 2) {
                        unrolled (ow.c.inner, 0, 15) {
                          vectorized (oc_block.c, 0, 3) {
                            conv.global(0, (c.outer.h.outer.fused/4), (oh.c.inner + ((c.outer.h.outer.fused % 4)*2)), ow.c.inner, oc_block.c) =(conv.global(0, (c.outer.h.outer.fused/4), (oh.c.inner + ((c.outer.h.outer.fused % 4)*2)), ow.c.inner, oc_block.c) + (int64(data_vec(0, ((ic.inner + (ic.outer*3))/3), ((oh.c.inner + ((c.outer.h.outer.fused % 4)*2)) + 0), ((ic.inner + (ic.outer*3)) % 3), (ow.c.inner + 0)))*int64(kernel_vec((c.outer.h.outer.fused/4), ((ic.inner + (ic.outer*3))/3), 0, 0, ((ic.inner + (ic.outer*3)) % 3), oc_block.c))))
                          }
                        }
                      }
                    }
                  }
                }
                for (oh.inner, 0, 2) {
                  for (ow, 0, 15) {
                    vectorized (oc_block, 0, 3) {
                      conv(0, (c.outer.h.outer.fused/4), (oh.inner + ((c.outer.h.outer.fused % 4)*2)), ow, oc_block) =conv.global(0, (c.outer.h.outer.fused/4), (oh.inner + ((c.outer.h.outer.fused % 4)*2)), ow, oc_block)
                    }
                  }
                }
              }
            }
            for (h.inner, 0, 2) {
              for (w.inner, 0, 15) {
                vectorized (c.inner, 0, 3) {
                  output_unpack(0, (c.inner + ((c.outer.h.outer.fused/4)*3)), (h.inner + ((c.outer.h.outer.fused % 4)*2)), w.inner) =conv(0, ((c.inner + ((c.outer.h.outer.fused/4)*3))/3), (h.inner + ((c.outer.h.outer.fused % 4)*2)), w.inner, ((c.inner + ((c.outer.h.outer.fused/4)*3)) % 3))
                }
              }
            }
          }
        }
      }
    }
  }
}
