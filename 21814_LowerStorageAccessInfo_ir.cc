produce T_strided_slice {
  parallel (ax0.ax1.fused, 0, 32) {
    for (ax2, 0, 32) {
      for (ax3, 0, 60) {
        T_strided_slice[((((ax0.ax1.fused*32) + ax2)*60) + ax3)] = placeholder[(((((ax0.ax1.fused*32) + ax2)*60) + ax3) + 61440)]
      }
    }
  }
}
