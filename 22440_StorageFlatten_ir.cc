produce T_strided_slice {
  parallel (ax0.ax1.fused, 0, 80) {
    for (ax2, 0, 2520) {
      T_strided_slice[((ax0.ax1.fused*2520) + ax2)] = placeholder[((((((ax0.ax1.fused/80)*144) + (ax0.ax1.fused % 80))*2520) + ax2) + 161280)]
    }
  }
}
