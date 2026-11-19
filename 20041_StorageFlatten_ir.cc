produce T_strided_slice {
  parallel (ax0.ax1.fused, 0, 64) {
    for (ax2, 0, 2520) {
      T_strided_slice[((ax0.ax1.fused*2520) + ax2)] = placeholder[(((((ax0.ax1.fused/64)*65) + (ax0.ax1.fused % 64))*2520) + ax2)]
    }
  }
}
