// attr [compute(data_vec, 0x4dc7f850)] realize_scope = ""
realize data_vec([0, 1], [0, 64], [0, 16], [0, 1], [0, 30]) {
  produce data_vec {
    parallel (C.h.fused, 0, 1024) {
      for (w, 0, 30) {
        data_vec(0, (C.h.fused/16), (C.h.fused % 16), 0, w) =placeholder(0, ((C.h.fused/16) + 0), (C.h.fused % 16), w)
      }
    }
  }
  // attr [compute(kernel_vec, 0x495d3f30)] realize_scope = ""
  realize kernel_vec([0, 1], [0, 64], [0, 1], [0, 1], [0, 1], [0, 1]) {
    produce kernel_vec {
      for (CI, 0, 64) {
        kernel_vec(0, CI, 0, 0, 0, 0) =placeholder((0 + 0), (CI + 0), 0, 0)
      }
    }
    // attr [compute(output_unpack, 0x4dc7fc70)] realize_scope = ""
    realize output_unpack([0, 1], [0, 1], [0, 16], [0, 30]) {
      produce output_unpack {
        parallel (c.outer.h.outer.fused, 0, 16) {
          // attr [compute(conv, 0x4dc81e50)] realize_scope = ""
          realize conv([0, 1], [(c.outer.h.outer.fused/16), 1], [(c.outer.h.outer.fused % 16), 1], [0, 30], [0, 1]) {
            produce conv {
              // attr [compute(conv.global, 0x46f7ef40)] realize_scope = "global"
              realize conv.global([0, 1], [(c.outer.h.outer.fused/16), 1], [(c.outer.h.outer.fused % 16), 1], [0, 30], [0, 1]) {
                produce conv.global {
                  unrolled (ow.c.inner.init, 0, 30) {
                    conv.global(0, (c.outer.h.outer.fused/16), (c.outer.h.outer.fused % 16), ow.c.inner.init, 0) =(int64)0
                  }
                  for (ic.outer, 0, 64) {
                    unrolled (ow.c.inner, 0, 30) {
                      conv.global(0, (c.outer.h.outer.fused/16), (c.outer.h.outer.fused % 16), ow.c.inner, 0) =(conv.global(0, (c.outer.h.outer.fused/16), (c.outer.h.outer.fused % 16), ow.c.inner, 0) + (int64(data_vec(0, ic.outer, ((c.outer.h.outer.fused % 16) + 0), 0, (ow.c.inner + 0)))*int64(kernel_vec((c.outer.h.outer.fused/16), ic.outer, 0, 0, 0, 0))))
                    }
                  }
                }
                for (ow, 0, 30) {
                  conv(0, (c.outer.h.outer.fused/16), (c.outer.h.outer.fused % 16), ow, 0) =conv.global(0, (c.outer.h.outer.fused/16), (c.outer.h.outer.fused % 16), ow, 0)
                }
              }
            }
            for (w.inner, 0, 30) {
              output_unpack(0, (c.outer.h.outer.fused/16), (c.outer.h.outer.fused % 16), w.inner) =conv(0, (c.outer.h.outer.fused/16), (c.outer.h.outer.fused % 16), w.inner, 0)
            }
          }
        }
      }
    }
  }
}
