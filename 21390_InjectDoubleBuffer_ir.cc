produce T_round_right_shift {
  parallel (ax0.ax1.fused, 0, 32) {
    for (ax2, 0, 64) {
      for (ax3, 0, 120) {
        T_round_right_shift[(((ax0.ax1.fused*7680) + (ax2*120)) + ax3)] = shift_right((placeholder[(((ax0.ax1.fused*7680) + (ax2*120)) + ax3)] + (int64((placeholder[ax0.ax1.fused] != 0))*shift_left((int64)1, int64((placeholder[ax0.ax1.fused] - 1))))), int64(placeholder[ax0.ax1.fused]))
      }
    }
  }
}
