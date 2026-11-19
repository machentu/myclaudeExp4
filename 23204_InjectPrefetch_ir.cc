// attr [compute(data_vec, 0x47d90e40)] realize_scope = ""
realize data_vec([0, 1], [0, 17], [0, 32], [0, 3], [0, 60]) {
  produce data_vec {
    parallel (C.h.fused, 0, 544) {
      for (c, 0, 3) {
        for (w, 0, 60) {
          data_vec(0, (C.h.fused/32), (C.h.fused % 32), c, w) =placeholder(0, (((C.h.fused/32)*3) + c), (C.h.fused % 32), w)
        }
      }
    }
  }
  // attr [compute(kernel_vec, 0x4cc31500)] realize_scope = ""
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
    // attr [compute(output_unpack, 0x4cc32f10)] realize_scope = ""
    realize output_unpack([0, 1], [0, 51], [0, 32], [0, 60]) {
      produce output_unpack {
        parallel (c.outer.h.outer.fused, 0, 544) {
          // attr [compute(conv, 0x4cc31db0)] realize_scope = ""
          realize conv([0, 1], [(c.outer.h.outer.fused/32), 1], [(c.outer.h.outer.fused % 32), 1], [0, 60], [0, 3]) {
            produce conv {
              // attr [compute(conv.global, 0x48413850)] realize_scope = "global"
              realize conv.global([0, 1], [(c.outer.h.outer.fused/32), 1], [(c.outer.h.outer.fused % 32), 1], [0, 60], [0, 3]) {
                produce conv.global {
                  for (ow.c.outer, 0, 2) {
                    unrolled (ow.c.inner.init, 0, 30) {
                      vectorized (oc_block.c.init, 0, 3) {
                        conv.global(0, (c.outer.h.outer.fused/32), (c.outer.h.outer.fused % 32), (ow.c.inner.init + (ow.c.outer*30)), oc_block.c.init) =(int64)0
                      }
                    }
                    for (ic.outer, 0, 17) {
                      for (ic.inner, 0, 3) {
                        unrolled (ow.c.inner, 0, 30) {
                          vectorized (oc_block.c, 0, 3) {
                            conv.global(0, (c.outer.h.outer.fused/32), (c.outer.h.outer.fused % 32), (ow.c.inner + (ow.c.outer*30)), oc_block.c) =(conv.global(0, (c.outer.h.outer.fused/32), (c.outer.h.outer.fused % 32), (ow.c.inner + (ow.c.outer*30)), oc_block.c) + (int64(data_vec(0, ((ic.inner + (ic.outer*3))/3), ((c.outer.h.outer.fused % 32) + 0), ((ic.inner + (ic.outer*3)) % 3), ((ow.c.inner + (ow.c.outer*30)) + 0)))*int64(kernel_vec((c.outer.h.outer.fused/32), ((ic.inner + (ic.outer*3))/3), 0, 0, ((ic.inner + (ic.outer*3)) % 3), oc_block.c))))
                          }
                        }
                      }
                    }
                  }
                }
                for (ow, 0, 60) {
                  vectorized (oc_block, 0, 3) {
                    conv(0, (c.outer.h.outer.fused/32), (c.outer.h.outer.fused % 32), ow, oc_block) =conv.global(0, (c.outer.h.outer.fused/32), (c.outer.h.outer.fused % 32), ow, oc_block)
                  }
                }
              }
            }
            for (w.outer, 0, 2) {
              for (w.inner, 0, 30) {
                vectorized (c.inner, 0, 3) {
                  output_unpack(0, (c.inner + ((c.outer.h.outer.fused/32)*3)), (c.outer.h.outer.fused % 32), (w.inner + (w.outer*30))) =conv(0, ((c.inner + ((c.outer.h.outer.fused/32)*3))/3), (c.outer.h.outer.fused % 32), (w.inner + (w.outer*30)), ((c.inner + ((c.outer.h.outer.fused/32)*3)) % 3))
                }
              }
            }
          }
        }
      }
    }
  }
}
