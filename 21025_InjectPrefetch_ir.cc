// attr [compute(data_vec, 0x36576be0)] realize_scope = ""
realize data_vec([0, 1], [0, 1], [0, 257], [0, 3], [0, 481]) {
  produce data_vec {
    parallel (C.h.fused, 0, 257) {
      for (c, 0, 3) {
        for (w, 0, 481) {
          data_vec(0, (C.h.fused/257), (C.h.fused % 257), c, w) =tvm_if_then_else((((((C.h.fused % 257) >= 1) && ((C.h.fused % 257) < 257)) && (w >= 1)) && (w < 481)), placeholder(0, (((C.h.fused/257)*3) + c), ((C.h.fused % 257) - 1), (w - 1)), (uint8)0)
        }
      }
    }
  }
  // attr [compute(kernel_vec, 0x302b2cd0)] realize_scope = ""
  realize kernel_vec([0, 2], [0, 1], [0, 3], [0, 3], [0, 3], [0, 8]) {
    produce kernel_vec {
      parallel (CO.h.fused, 0, 6) {
        for (w, 0, 3) {
          for (ci, 0, 3) {
            vectorized (co, 0, 8) {
              kernel_vec((CO.h.fused/3), 0, (CO.h.fused % 3), w, ci, co) =placeholder((((CO.h.fused/3)*8) + co), ((0*3) + ci), (CO.h.fused % 3), w)
            }
          }
        }
      }
    }
    // attr [compute(output_unpack, 0x3c281610)] realize_scope = ""
    realize output_unpack([0, 1], [0, 16], [0, 128], [0, 240]) {
      produce output_unpack {
        parallel (c.outer.h.fused, 0, 256) {
          // attr [compute(conv, 0x3c2801c0)] realize_scope = ""
          realize conv([0, 1], [(c.outer.h.fused/128), 1], [(c.outer.h.fused % 128), 1], [0, 240], [0, 8]) {
            produce conv {
              for (ow.outer, 0, 8) {
                // attr [compute(conv.global, 0x38dfcf60)] realize_scope = "global"
                realize conv.global([0, 1], [(c.outer.h.fused/128), 1], [(c.outer.h.fused % 128), 1], [(ow.outer*30), 30], [0, 8]) {
                  produce conv.global {
                    unrolled (ow.c.inner.init, 0, 30) {
                      vectorized (oc_block.c.init, 0, 8) {
                        conv.global(0, (c.outer.h.fused/128), (c.outer.h.fused % 128), (ow.c.inner.init + (ow.outer*30)), oc_block.c.init) =(int64)0
                      }
                    }
                    for (kh, 0, 3) {
                      for (kw, 0, 3) {
                        for (ic.inner, 0, 3) {
                          unrolled (ow.c.inner, 0, 30) {
                            vectorized (oc_block.c, 0, 8) {
                              conv.global(0, (c.outer.h.fused/128), (c.outer.h.fused % 128), (ow.c.inner + (ow.outer*30)), oc_block.c) =(conv.global(0, (c.outer.h.fused/128), (c.outer.h.fused % 128), (ow.c.inner + (ow.outer*30)), oc_block.c) + (int64(data_vec(0, (ic.inner/3), (((c.outer.h.fused % 128)*2) + kh), (ic.inner % 3), (((ow.c.inner + (ow.outer*30))*2) + kw)))*int64(kernel_vec((c.outer.h.fused/128), (ic.inner/3), kh, kw, (ic.inner % 3), oc_block.c))))
                            }
                          }
                        }
                      }
                    }
                  }
                  for (ow.inner, 0, 30) {
                    vectorized (oc_block, 0, 8) {
                      conv(0, (c.outer.h.fused/128), (c.outer.h.fused % 128), (ow.inner + (ow.outer*30)), oc_block) =conv.global(0, (c.outer.h.fused/128), (c.outer.h.fused % 128), (ow.inner + (ow.outer*30)), oc_block)
                    }
                  }
                }
              }
            }
            for (w.outer, 0, 8) {
              for (w.inner, 0, 30) {
                vectorized (c.inner, 0, 8) {
                  output_unpack(0, (c.inner + ((c.outer.h.fused/128)*8)), (c.outer.h.fused % 128), (w.inner + (w.outer*30))) =conv(0, ((c.inner + ((c.outer.h.fused/128)*8))/8), (c.outer.h.fused % 128), (w.inner + (w.outer*30)), ((c.inner + ((c.outer.h.fused/128)*8)) % 8))
                }
              }
            }
          }
        }
      }
    }
  }
}
