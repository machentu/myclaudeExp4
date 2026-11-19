produce T_add {
  parallel (ax0.ax1.fused, 0, 32) {
    for (ax2, 0, 32) {
      for (ax3, 0, 60) {
        T_add[(((ax0.ax1.fused*1920) + (ax2*60)) + ax3)] = int64((placeholder[(((ax0.ax1.fused*1920) + (ax2*60)) + ax3)] + placeholder[ax0.ax1.fused]))
      }
    }
  }
}
