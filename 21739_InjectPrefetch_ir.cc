// attr [compute(data_vec, 0x3605bcc0)] realize_scope = ""
realize data_vec([0, 1], [0, 8], [0, 34], [0, 8], [0, 62]) {
  produce data_vec {
    parallel (C.h.fused, 0, 272) {
      for (c, 0, 8) {
        for (w, 0, 62) {
          data_vec(0, (C.h.fused/34), (C.h.fused % 34), c, w) =tvm_if_then_else((((((C.h.fused % 34) >= 1) && ((C.h.fused % 34) < 33)) && (w >= 1)) && (w < 61)), placeholder(0, (((C.h.fused/34)*8) + c), ((C.h.fused % 34) - 1), (w - 1)), (int8)0)
        }
      }
    }
  }
  // attr [compute(kernel_vec, 0x3605aa90)] realize_scope = ""
  realize kernel_vec([0, 10], [0, 8], [0, 3], [0, 3], [0, 8], [0, 8]) {
    produce kernel_vec {
      parallel (CO.h.fused, 0, 30) {
        for (CI, 0, 8) {
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
    // attr [compute(output_unpack, 0x3e0a8020)] realize_scope = ""
    realize output_unpack([0, 1], [0, 80], [0, 32], [0, 60]) {
      produce output_unpack {
        parallel (c.outer.h.fused, 0, 320) {
          // attr [compute(conv, 0x35fea830)] realize_scope = ""
          realize conv([0, 1], [(c.outer.h.fused/32), 1], [(c.outer.h.fused % 32), 1], [0, 60], [0, 8]) {
            produce conv {
              for (ow.outer, 0, 2) {
                // attr [compute(conv.global, 0x374810e0)] realize_scope = "global"
                realize conv.global([0, 1], [(c.outer.h.fused/32), 1], [(c.outer.h.fused % 32), 1], [(ow.outer*30), 30], [0, 8]) {
                  produce conv.global {
                    unrolled (ow.c.inner.init, 0, 30) {
                      vectorized (oc_block.c.init, 0, 8) {
                        conv.global(0, (c.outer.h.fused/32), (c.outer.h.fused % 32), (ow.c.inner.init + (ow.outer*30)), oc_block.c.init) =(int64)0
                      }
                    }
                    for (ic.outer, 0, 8) {
                      for (kh, 0, 3) {
                        for (kw, 0, 3) {
                          for (ic.inner, 0, 8) {
                            unrolled (ow.c.inner, 0, 30) {
                              vectorized (oc_block.c, 0, 8) {
                                conv.global(0, (c.outer.h.fused/32), (c.outer.h.fused % 32), (ow.c.inner + (ow.outer*30)), oc_block.c) =(conv.global(0, (c.outer.h.fused/32), (c.outer.h.fused % 32), (ow.c.inner + (ow.outer*30)), oc_block.c) + (int64(data_vec(0, ((ic.inner + (ic.outer*8))/8), ((c.outer.h.fused % 32) + kh), ((ic.inner + (ic.outer*8)) % 8), ((ow.c.inner + (ow.outer*30)) + kw)))*int64(kernel_vec((c.outer.h.fused/32), ((ic.inner + (ic.outer*8))/8), kh, kw, ((ic.inner + (ic.outer*8)) % 8), oc_block.c))))
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                  for (ow.inner, 0, 30) {
                    vectorized (oc_block, 0, 8) {
                      conv(0, (c.outer.h.fused/32), (c.outer.h.fused % 32), (ow.inner + (ow.outer*30)), oc_block) =conv.global(0, (c.outer.h.fused/32), (c.outer.h.fused % 32), (ow.inner + (ow.outer*30)), oc_block)
                    }
                  }
                }
              }
            }
            for (w.outer, 0, 2) {
              for (w.inner, 0, 30) {
                vectorized (c.inner, 0, 8) {
                  output_unpack(0, (c.inner + ((c.outer.h.fused/32)*8)), (c.outer.h.fused % 32), (w.inner + (w.outer*30))) =conv(0, ((c.inner + ((c.outer.h.fused/32)*8))/8), (c.outer.h.fused % 32), (w.inner + (w.outer*30)), ((c.inner + ((c.outer.h.fused/32)*8)) % 8))
                }
              }
            }
          }
        }
      }
    }
  }
}
