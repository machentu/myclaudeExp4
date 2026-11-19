produce T_add {
  parallel (ax0.ax1.fused, 0, 128) {
    for (ax2, 0, 8) {
      for (ax3, 0, 4) {
        T_add[((((ax0.ax1.fused*8) + ax2)*4) + ax3)] = (placeholder[((((ax0.ax1.fused*8) + ax2)*4) + ax3)] + placeholder[((((ax0.ax1.fused*8) + ax2)*4) + ax3)])
      }
    }
  }
}
