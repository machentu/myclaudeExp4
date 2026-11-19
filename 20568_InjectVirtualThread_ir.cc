produce T_round_right_shift {
  parallel (ax0.ax1.fused, 0, 256) {
    for (ax2, 0, 16) {
      for (ax3, 0, 30) {
        T_round_right_shift[(((ax0.ax1.fused*480) + (ax2*30)) + ax3)] = shift_right((placeholder[(((ax0.ax1.fused*480) + (ax2*30)) + ax3)] + (int64((placeholder[ax0.ax1.fused] != 0))*shift_left((int64)1, int64((placeholder[ax0.ax1.fused] - 1))))), int64(placeholder[ax0.ax1.fused]))
      }
    }
  }
}
