// attr [compute(data_vec, 0x3f371920)] realize_scope = ""
realize data_vec([0, 1], [0, 64], [0, 8], [0, 1], [0, 15]) {
  produce data_vec {
    parallel (C.h.fused, 0, 512) {
      for (w, 0, 15) {
        data_vec(0, (C.h.fused/8), (C.h.fused % 8), 0, w) =placeholder(0, ((C.h.fused/8) + 0), (C.h.fused % 8), w)
      }
    }
  }
  // attr [compute(kernel_vec, 0x3f371d40)] realize_scope = ""
  realize kernel_vec([0, 1], [0, 64], [0, 1], [0, 1], [0, 1], [0, 1]) {
    produce kernel_vec {
      for (CI, 0, 64) {
        kernel_vec(0, CI, 0, 0, 0, 0) =placeholder((0 + 0), (CI + 0), 0, 0)
      }
    }
    // attr [compute(output_unpack, 0x48078f60)] realize_scope = ""
    realize output_unpack([0, 1], [0, 1], [0, 8], [0, 15]) {
      produce output_unpack {
        parallel (c.outer.h.outer.fused, 0, 4) {
          // attr [compute(conv, 0x3f374b30)] realize_scope = ""
          realize conv([0, 1], [(c.outer.h.outer.fused/4), 1], [((c.outer.h.outer.fused % 4)*2), 2], [0, 15], [0, 1]) {
            produce conv {
              // attr [compute(conv.global, 0x4d992510)] realize_scope = "global"
              realize conv.global([0, 1], [(c.outer.h.outer.fused/4), 1], [((c.outer.h.outer.fused % 4)*2), 2], [0, 15], [0, 1]) {
                produce conv.global {
                  unrolled (oh.c.inner.init, 0, 2) {
                    unrolled (ow.c.inner.init, 0, 15) {
                      conv.global(0, (c.outer.h.outer.fused/4), (oh.c.inner.init + ((c.outer.h.outer.fused % 4)*2)), ow.c.inner.init, 0) =(int64)0
                    }
                  }
                  for (ic.outer, 0, 64) {
                    unrolled (oh.c.inner, 0, 2) {
                      unrolled (ow.c.inner, 0, 15) {
                        conv.global(0, (c.outer.h.outer.fused/4), (oh.c.inner + ((c.outer.h.outer.fused % 4)*2)), ow.c.inner, 0) =(conv.global(0, (c.outer.h.outer.fused/4), (oh.c.inner + ((c.outer.h.outer.fused % 4)*2)), ow.c.inner, 0) + (int64(data_vec(0, ic.outer, ((oh.c.inner + ((c.outer.h.outer.fused % 4)*2)) + 0), 0, (ow.c.inner + 0)))*int64(kernel_vec((c.outer.h.outer.fused/4), ic.outer, 0, 0, 0, 0))))
                      }
                    }
                  }
                }
                for (oh.inner, 0, 2) {
                  for (ow, 0, 15) {
                    conv(0, (c.outer.h.outer.fused/4), (oh.inner + ((c.outer.h.outer.fused % 4)*2)), ow, 0) =conv.global(0, (c.outer.h.outer.fused/4), (oh.inner + ((c.outer.h.outer.fused % 4)*2)), ow, 0)
                  }
                }
              }
            }
            for (h.inner, 0, 2) {
              for (w.inner, 0, 15) {
                output_unpack(0, (c.outer.h.outer.fused/4), (h.inner + ((c.outer.h.outer.fused % 4)*2)), w.inner) =conv(0, (c.outer.h.outer.fused/4), (h.inner + ((c.outer.h.outer.fused % 4)*2)), w.inner, 0)
              }
            }
          }
        }
      }
    }
  }
}
