// attr [compute(data_vec, 0x45607310)] realize_scope = ""
realize data_vec([0, 1], [0, 64], [0, 32], [0, 1], [0, 60]) {
  produce data_vec {
    parallel (C.h.fused, 0, 2048) {
      for (w, 0, 60) {
        data_vec(0, (C.h.fused/32), (C.h.fused % 32), 0, w) =placeholder(0, ((C.h.fused/32) + 0), (C.h.fused % 32), w)
      }
    }
  }
  // attr [compute(kernel_vec, 0x499b74c0)] realize_scope = ""
  realize kernel_vec([0, 1], [0, 64], [0, 1], [0, 1], [0, 1], [0, 1]) {
    produce kernel_vec {
      for (CI, 0, 64) {
        kernel_vec(0, CI, 0, 0, 0, 0) =placeholder((0 + 0), (CI + 0), 0, 0)
      }
    }
    // attr [compute(output_unpack, 0x477fe420)] realize_scope = ""
    realize output_unpack([0, 1], [0, 1], [0, 32], [0, 60]) {
      produce output_unpack {
        parallel (c.outer.h.outer.fused, 0, 32) {
          // attr [compute(conv, 0x4560a550)] realize_scope = ""
          realize conv([0, 1], [(c.outer.h.outer.fused/32), 1], [(c.outer.h.outer.fused % 32), 1], [0, 60], [0, 1]) {
            produce conv {
              // attr [compute(conv.global, 0x45607760)] realize_scope = "global"
              realize conv.global([0, 1], [(c.outer.h.outer.fused/32), 1], [(c.outer.h.outer.fused % 32), 1], [0, 60], [0, 1]) {
                produce conv.global {
                  for (ow.c.outer, 0, 2) {
                    unrolled (ow.c.inner.init, 0, 30) {
                      conv.global(0, (c.outer.h.outer.fused/32), (c.outer.h.outer.fused % 32), (ow.c.inner.init + (ow.c.outer*30)), 0) =(int64)0
                    }
                    for (ic.outer, 0, 64) {
                      unrolled (ow.c.inner, 0, 30) {
                        conv.global(0, (c.outer.h.outer.fused/32), (c.outer.h.outer.fused % 32), (ow.c.inner + (ow.c.outer*30)), 0) =(conv.global(0, (c.outer.h.outer.fused/32), (c.outer.h.outer.fused % 32), (ow.c.inner + (ow.c.outer*30)), 0) + (int64(data_vec(0, ic.outer, ((c.outer.h.outer.fused % 32) + 0), 0, ((ow.c.inner + (ow.c.outer*30)) + 0)))*int64(kernel_vec((c.outer.h.outer.fused/32), ic.outer, 0, 0, 0, 0))))
                      }
                    }
                  }
                }
                for (ow, 0, 60) {
                  conv(0, (c.outer.h.outer.fused/32), (c.outer.h.outer.fused % 32), ow, 0) =conv.global(0, (c.outer.h.outer.fused/32), (c.outer.h.outer.fused % 32), ow, 0)
                }
              }
            }
            for (w.outer, 0, 2) {
              for (w.inner, 0, 30) {
                output_unpack(0, (c.outer.h.outer.fused/32), (c.outer.h.outer.fused % 32), (w.inner + (w.outer*30))) =conv(0, (c.outer.h.outer.fused/32), (c.outer.h.outer.fused % 32), (w.inner + (w.outer*30)), 0)
              }
            }
          }
        }
      }
    }
  }
}
