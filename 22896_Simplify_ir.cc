produce T_add {
  parallel (ax0.ax1.fused, 0, 32) {
    for (ax2, 0, 8) {
      for (ax3, 0, 15) {
        T_add[((((ax0.ax1.fused*8) + ax2)*15) + ax3)] = int64((placeholder[((((ax0.ax1.fused*8) + ax2)*15) + ax3)] + placeholder[ax0.ax1.fused]))
      }
    }
  }
}
