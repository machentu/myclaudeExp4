produce T_relu {
  parallel (ax0.ax1.fused, 0, 16) {
    for (ax2, 0, 128) {
      for (ax3, 0, 240) {
        T_relu[(((ax0.ax1.fused*30720) + (ax2*240)) + ax3)] = max(placeholder[(((ax0.ax1.fused*30720) + (ax2*240)) + ax3)], (int8)0)
      }
    }
  }
}
