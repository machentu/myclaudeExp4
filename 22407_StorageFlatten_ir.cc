produce T_reshape {
  parallel (ax0.ax1.fused, 0, 65) {
    for (ax2, 0, 480) {
      T_reshape[((ax0.ax1.fused*480) + ax2)] = placeholder[((((((ax0.ax1.fused + (ax2/480)) % 65)*16) + ((ax2/30) % 16))*30) + (ax2 % 30))]
    }
  }
}
