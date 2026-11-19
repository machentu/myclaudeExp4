produce T_add {
  parallel (ax0.ax1.fused, 0, 32) {
    for (ax2, 0, 16) {
      for (ax3, 0, 30) {
        T_add[((((ax0.ax1.fused*16) + ax2)*30) + ax3)] = int64((placeholder[((((ax0.ax1.fused*16) + ax2)*30) + ax3)] + placeholder[(ax0.ax1.fused % 32)]))
      }
    }
  }
}
