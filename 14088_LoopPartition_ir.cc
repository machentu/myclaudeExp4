produce T_round {
  parallel (ax0.ax1.fused, 0, 384) {
    for (ax2, 0, 16) {
      for (ax3, 0, 8) {
        T_round[(((ax0.ax1.fused*128) + (ax2*8)) + ax3)] = round(placeholder[(((ax0.ax1.fused*128) + (ax2*8)) + ax3)])
      }
    }
  }
}
