produce T_batch_matmul {
  // attr [iter_var(ax0.ax1.outer.fused.ax2.outer.fused, Range(min=0, extent=64))] pragma_auto_unroll_max_step = 16
  parallel (ax0.ax1.outer.fused.ax2.outer.fused, 0, 64) {
    // attr [T_batch_matmul.rf] storage_scope = "global"
    allocate T_batch_matmul.rf[int64 * 16 * 1 * 8 * 1]
    produce T_batch_matmul.rf {
      vectorized (k.inner, 0, 16) {
        for (m.n.fused, 0, 8) {
          T_batch_matmul.rf[((k.inner*8) + m.n.fused)] = (int64)0
          for (k.outer, 0, 32) {
            T_batch_matmul.rf[((k.inner*8) + m.n.fused)] = (T_batch_matmul.rf[((k.inner*8) + m.n.fused)] + (int64(placeholder[((((ax0.ax1.outer.fused.ax2.outer.fused*4096) + (m.n.fused*512)) + (k.outer*16)) + k.inner)])*int64(placeholder[((k.outer*16) + k.inner)])))
          }
        }
      }
    }
    for (ax1.inner.ax2.inner.fused, 0, 8) {
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (int64)0
      for (k.inner.v, 0, 16) {
        T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[((k.inner.v*8) + ax1.inner.ax2.inner.fused)])
      }
    }
  }
}
