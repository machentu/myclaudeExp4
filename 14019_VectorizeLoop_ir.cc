produce T_round_right_shift {
  parallel (ax0.ax1.fused, 0, 384) {
    for (ax2, 0, 16) {
      for (ax3, 0, 8) {
        T_round_right_shift[(((ax0.ax1.fused*128) + (ax2*8)) + ax3)] = shift_right((placeholder[(((ax0.ax1.fused*128) + (ax2*8)) + ax3)] + (int64((placeholder[0] != 0))*shift_left((int64)1, int64((placeholder[0] - 1))))), int64(placeholder[0]))
      }
    }
  }
}
