// attr [compute(data_vec, 0x481241a0)] realize_scope = ""
realize data_vec([0, 4], [0, 16], [0, 1], [0, 1], [0, 2520]) {
  produce data_vec {
    for (n, 0, 4) {
      parallel (C.h.fused, 0, 16) {
        for (w, 0, 2520) {
          data_vec(n, C.h.fused, 0, 0, w) =placeholder(n, (C.h.fused + 0), 0, w)
        }
      }
    }
  }
  // attr [compute(kernel_vec, 0x490aa2b0)] realize_scope = ""
  realize kernel_vec([0, 1], [0, 16], [0, 1], [0, 1], [0, 1], [0, 1]) {
    produce kernel_vec {
      for (CI, 0, 16) {
        kernel_vec(0, CI, 0, 0, 0, 0) =placeholder((0 + 0), (CI + 0), 0, 0)
      }
    }
    // attr [compute(output_unpack, 0x48d9f5b0)] realize_scope = ""
    realize output_unpack([0, 4], [0, 1], [0, 1], [0, 2520]) {
      produce output_unpack {
        for (n, 0, 4) {
          // attr [compute(conv, 0x488439a0)] realize_scope = ""
          realize conv([n, 1], [0, 1], [0, 1], [0, 2520], [0, 1]) {
            produce conv {
              // attr [compute(conv.global, 0x48cff410)] realize_scope = "global"
              realize conv.global([n, 1], [0, 1], [0, 1], [0, 2520], [0, 1]) {
                produce conv.global {
                  for (ow.c.outer, 0, 84) {
                    unrolled (ow.c.inner.init, 0, 30) {
                      conv.global(n, 0, 0, (ow.c.inner.init + (ow.c.outer*30)), 0) =(int64)0
                    }
                    for (ic.outer, 0, 16) {
                      unrolled (ow.c.inner, 0, 30) {
                        conv.global(n, 0, 0, (ow.c.inner + (ow.c.outer*30)), 0) =(conv.global(n, 0, 0, (ow.c.inner + (ow.c.outer*30)), 0) + (int64(data_vec(n, ic.outer, (0 + 0), 0, ((ow.c.inner + (ow.c.outer*30)) + 0)))*int64(kernel_vec(0, ic.outer, 0, 0, 0, 0))))
                      }
                    }
                  }
                }
                for (ow, 0, 2520) {
                  conv(n, 0, 0, ow, 0) =conv.global(n, 0, 0, ow, 0)
                }
              }
            }
            for (w.outer, 0, 84) {
              for (w.inner, 0, 30) {
                output_unpack(n, 0, 0, (w.inner + (w.outer*30))) =conv(n, 0, 0, (w.inner + (w.outer*30)), 0)
              }
            }
          }
        }
      }
    }
  }
}
