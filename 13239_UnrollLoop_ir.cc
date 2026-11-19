produce T_batch_matmul {
  parallel (ax0.ax1.outer.fused.ax2.outer.fused, 0, 64) {
    // attr [T_batch_matmul.rf] storage_scope = "global"
    allocate T_batch_matmul.rf[int64 * 128]
    produce T_batch_matmul.rf {
      for (m.n.fused, 0, 8) {
        T_batch_matmul.rf[ramp(m.n.fused, 8, 16)] = x16((int64)0)
        for (k.outer, 0, 32) {
          T_batch_matmul.rf[ramp(m.n.fused, 8, 16)] = (T_batch_matmul.rf[ramp(m.n.fused, 8, 16)] + (int64x16(placeholder[ramp((((ax0.ax1.outer.fused.ax2.outer.fused*4096) + (m.n.fused*512)) + (k.outer*16)), 1, 16)])*int64x16(placeholder[ramp((k.outer*16), 1, 16)])))
        }
      }
    }
    for (ax1.inner.ax2.inner.fused, 0, 8) {
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (int64)0
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[((0*8) + ax1.inner.ax2.inner.fused)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[((1*8) + ax1.inner.ax2.inner.fused)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[((2*8) + ax1.inner.ax2.inner.fused)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[((3*8) + ax1.inner.ax2.inner.fused)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[((4*8) + ax1.inner.ax2.inner.fused)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[((5*8) + ax1.inner.ax2.inner.fused)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[((6*8) + ax1.inner.ax2.inner.fused)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[((7*8) + ax1.inner.ax2.inner.fused)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[((8*8) + ax1.inner.ax2.inner.fused)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[((9*8) + ax1.inner.ax2.inner.fused)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[((10*8) + ax1.inner.ax2.inner.fused)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[((11*8) + ax1.inner.ax2.inner.fused)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[((12*8) + ax1.inner.ax2.inner.fused)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[((13*8) + ax1.inner.ax2.inner.fused)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[((14*8) + ax1.inner.ax2.inner.fused)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[((15*8) + ax1.inner.ax2.inner.fused)])
    }
  }
}
