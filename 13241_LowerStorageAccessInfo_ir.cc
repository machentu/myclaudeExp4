produce T_batch_matmul {
  parallel (ax0.ax1.outer.fused.ax2.outer.fused, 0, 64) {
    // attr [T_batch_matmul.rf] storage_scope = "global"
    allocate T_batch_matmul.rf[int64 * 128]
    produce T_batch_matmul.rf {
      for (m.n.fused, 0, 8) {
        T_batch_matmul.rf[ramp(m.n.fused, 8, 16)] = x16((int64)0)
        for (k.outer, 0, 32) {
          T_batch_matmul.rf[ramp(m.n.fused, 8, 16)] = (T_batch_matmul.rf[ramp(m.n.fused, 8, 16)] + (int64x16(placeholder[ramp((((((ax0.ax1.outer.fused.ax2.outer.fused*8) + m.n.fused)*32) + k.outer)*16), 1, 16)])*int64x16(placeholder[ramp((k.outer*16), 1, 16)])))
        }
      }
    }
    for (ax1.inner.ax2.inner.fused, 0, 8) {
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (int64)0
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[ax1.inner.ax2.inner.fused])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[(ax1.inner.ax2.inner.fused + 8)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[(ax1.inner.ax2.inner.fused + 16)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[(ax1.inner.ax2.inner.fused + 24)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[(ax1.inner.ax2.inner.fused + 32)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[(ax1.inner.ax2.inner.fused + 40)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[(ax1.inner.ax2.inner.fused + 48)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[(ax1.inner.ax2.inner.fused + 56)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[(ax1.inner.ax2.inner.fused + 64)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[(ax1.inner.ax2.inner.fused + 72)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[(ax1.inner.ax2.inner.fused + 80)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[(ax1.inner.ax2.inner.fused + 88)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[(ax1.inner.ax2.inner.fused + 96)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[(ax1.inner.ax2.inner.fused + 104)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[(ax1.inner.ax2.inner.fused + 112)])
      T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] = (T_batch_matmul[((ax0.ax1.outer.fused.ax2.outer.fused*8) + ax1.inner.ax2.inner.fused)] + T_batch_matmul.rf[(ax1.inner.ax2.inner.fused + 120)])
    }
  }
}
