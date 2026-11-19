produce T_reshape {
  parallel (ax0.ax1.fused, 0, 51) {
    for (ax2, 0, 2520) {
      T_reshape[((ax0.ax1.fused*2520) + ax2)] = placeholder[(((((((ax0.ax1.fused + (ax2/2520))/3) % 17)*3) + ((ax0.ax1.fused + (ax2/2520)) % 3))*2520) + (ax2 % 2520))]
    }
  }
}
