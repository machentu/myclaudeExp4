// attr [compute(T_batch_matmul, 0x26622970)] realize_scope = ""
realize T_batch_matmul([0, 1], [0, 512], [0, 1]) {
  produce T_batch_matmul {
    // attr [iter_var(ax0.ax1.outer.fused.ax2.outer.fused, Range(min=0, extent=64))] pragma_auto_unroll_max_step = 16
    parallel (ax0.ax1.outer.fused.ax2.outer.fused, 0, 64) {
      // attr [compute(T_batch_matmul.rf, 0x17bd6520)] realize_scope = ""
      realize T_batch_matmul.rf([0, 16], [(ax0.ax1.outer.fused.ax2.outer.fused/64), 1], [((ax0.ax1.outer.fused.ax2.outer.fused % 64)*8), 8], [0, 1]) {
        produce T_batch_matmul.rf {
          vectorized (k.inner, 0, 16) {
            for (m.n.fused, 0, 8) {
              T_batch_matmul.rf(k.inner, (ax0.ax1.outer.fused.ax2.outer.fused/64), (m.n.fused + ((ax0.ax1.outer.fused.ax2.outer.fused % 64)*8)), 0) =(int64)0
              for (k.outer, 0, 32) {
                T_batch_matmul.rf(k.inner, (ax0.ax1.outer.fused.ax2.outer.fused/64), (m.n.fused + ((ax0.ax1.outer.fused.ax2.outer.fused % 64)*8)), 0) =(T_batch_matmul.rf(k.inner, (ax0.ax1.outer.fused.ax2.outer.fused/64), (m.n.fused + ((ax0.ax1.outer.fused.ax2.outer.fused % 64)*8)), 0) + (int64(placeholder((ax0.ax1.outer.fused.ax2.outer.fused/64), (m.n.fused + ((ax0.ax1.outer.fused.ax2.outer.fused % 64)*8)), (k.inner + (k.outer*16))))*int64(placeholder((ax0.ax1.outer.fused.ax2.outer.fused/64), 0, (k.inner + (k.outer*16))))))
              }
            }
          }
        }
        for (ax1.inner.ax2.inner.fused, 0, 8) {
          T_batch_matmul((ax0.ax1.outer.fused.ax2.outer.fused/64), (ax1.inner.ax2.inner.fused + ((ax0.ax1.outer.fused.ax2.outer.fused % 64)*8)), 0) =(int64)0
          for (k.inner.v, 0, 16) {
            T_batch_matmul((ax0.ax1.outer.fused.ax2.outer.fused/64), (ax1.inner.ax2.inner.fused + ((ax0.ax1.outer.fused.ax2.outer.fused % 64)*8)), 0) =(T_batch_matmul((ax0.ax1.outer.fused.ax2.outer.fused/64), (ax1.inner.ax2.inner.fused + ((ax0.ax1.outer.fused.ax2.outer.fused % 64)*8)), 0) + T_batch_matmul.rf(k.inner.v, (ax0.ax1.outer.fused.ax2.outer.fused/64), (ax1.inner.ax2.inner.fused + ((ax0.ax1.outer.fused.ax2.outer.fused % 64)*8)), 0))
          }
        }
      }
    }
  }
}
