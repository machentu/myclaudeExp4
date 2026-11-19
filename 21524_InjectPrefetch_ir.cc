// attr [compute(data_vec, 0x4ccb5520)] realize_scope = ""
realize data_vec([0, 1], [0, 4], [0, 64], [0, 8], [0, 120]) {
  produce data_vec {
    parallel (C.h.fused, 0, 256) {
      for (c, 0, 8) {
        for (w, 0, 120) {
          data_vec(0, (C.h.fused/64), (C.h.fused % 64), c, w) =placeholder(0, (((C.h.fused/64)*8) + c), (C.h.fused % 64), w)
        }
      }
    }
  }
  // attr [compute(kernel_vec, 0x48b1de00)] realize_scope = ""
  realize kernel_vec([0, 4], [0, 4], [0, 1], [0, 1], [0, 8], [0, 8]) {
    produce kernel_vec {
      parallel (CO.h.fused, 0, 4) {
        for (CI, 0, 4) {
          for (ci, 0, 8) {
            vectorized (co, 0, 8) {
              kernel_vec(CO.h.fused, CI, 0, 0, ci, co) =placeholder(((CO.h.fused*8) + co), ((CI*8) + ci), 0, 0)
            }
          }
        }
      }
    }
    // attr [compute(output_unpack, 0x43a4ab00)] realize_scope = ""
    realize output_unpack([0, 1], [0, 32], [0, 64], [0, 120]) {
      produce output_unpack {
        parallel (c.outer.h.outer.fused, 0, 256) {
          // attr [compute(conv, 0x43a499a0)] realize_scope = ""
          realize conv([0, 1], [(c.outer.h.outer.fused/64), 1], [(c.outer.h.outer.fused % 64), 1], [0, 120], [0, 8]) {
            produce conv {
              // attr [compute(conv.global, 0x43a4d6d0)] realize_scope = "global"
              realize conv.global([0, 1], [(c.outer.h.outer.fused/64), 1], [(c.outer.h.outer.fused % 64), 1], [0, 120], [0, 8]) {
                produce conv.global {
                  for (ow.c.outer, 0, 4) {
                    unrolled (ow.c.inner.init, 0, 30) {
                      vectorized (oc_block.c.init, 0, 8) {
                        conv.global(0, (c.outer.h.outer.fused/64), (c.outer.h.outer.fused % 64), (ow.c.inner.init + (ow.c.outer*30)), oc_block.c.init) =(int64)0
                      }
                    }
                    for (ic.outer, 0, 4) {
                      for (ic.inner, 0, 8) {
                        unrolled (ow.c.inner, 0, 30) {
                          vectorized (oc_block.c, 0, 8) {
                            conv.global(0, (c.outer.h.outer.fused/64), (c.outer.h.outer.fused % 64), (ow.c.inner + (ow.c.outer*30)), oc_block.c) =(conv.global(0, (c.outer.h.outer.fused/64), (c.outer.h.outer.fused % 64), (ow.c.inner + (ow.c.outer*30)), oc_block.c) + (int64(data_vec(0, ((ic.inner + (ic.outer*8))/8), ((c.outer.h.outer.fused % 64) + 0), ((ic.inner + (ic.outer*8)) % 8), ((ow.c.inner + (ow.c.outer*30)) + 0)))*int64(kernel_vec((c.outer.h.outer.fused/64), ((ic.inner + (ic.outer*8))/8), 0, 0, ((ic.inner + (ic.outer*8)) % 8), oc_block.c))))
                          }
                        }
                      }
                    }
                  }
                }
                for (ow, 0, 120) {
                  vectorized (oc_block, 0, 8) {
                    conv(0, (c.outer.h.outer.fused/64), (c.outer.h.outer.fused % 64), ow, oc_block) =conv.global(0, (c.outer.h.outer.fused/64), (c.outer.h.outer.fused % 64), ow, oc_block)
                  }
                }
              }
            }
            for (w.outer, 0, 4) {
              for (w.inner, 0, 30) {
                vectorized (c.inner, 0, 8) {
                  output_unpack(0, (c.inner + ((c.outer.h.outer.fused/64)*8)), (c.outer.h.outer.fused % 64), (w.inner + (w.outer*30))) =conv(0, ((c.inner + ((c.outer.h.outer.fused/64)*8))/8), (c.outer.h.outer.fused % 64), (w.inner + (w.outer*30)), ((c.inner + ((c.outer.h.outer.fused/64)*8)) % 8))
                }
              }
            }
          }
        }
      }
    }
  }
}
