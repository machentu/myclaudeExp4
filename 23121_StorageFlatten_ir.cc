produce T_reshape {
  parallel (ax0.ax1.fused, 0, 51) {
    for (ax2, 0, 1920) {
      T_reshape[((ax0.ax1.fused*1920) + ax2)] = placeholder[((((((ax0.ax1.fused + (ax2/1920)) % 51)*32) + ((ax2/60) % 32))*60) + (ax2 % 60))]
    }
  }
}
