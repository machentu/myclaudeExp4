produce T_add {
  parallel (ax0.ax1.fused, 0, 64) {
    for (ax2, 0, 32) {
      for (ax3, 0, 16) {
        T_add[(((ax0.ax1.fused*512) + (ax2*16)) + ax3)] = int64((placeholder[(((ax0.ax1.fused*512) + (ax2*16)) + ax3)] + placeholder[ax0.ax1.fused]))
      }
    }
  }
}
