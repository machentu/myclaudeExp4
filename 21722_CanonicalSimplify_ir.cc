produce T_add {
  parallel (ax0.ax1.fused, 0, 16) {
    for (ax2, 0, 64) {
      for (ax3, 0, 120) {
        T_add[(((ax0.ax1.fused*7680) + (ax2*120)) + ax3)] = (int32(placeholder[(((ax0.ax1.fused*7680) + (ax2*120)) + ax3)]) + int32(placeholder[(((ax0.ax1.fused*7680) + (ax2*120)) + ax3)]))
      }
    }
  }
}
