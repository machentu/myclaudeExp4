produce T_round_right_shift {
  parallel (ax0.ax1.fused, 0, 144) {
    for (ax2, 0, 2520) {
      T_round_right_shift[((ax0.ax1.fused*2520) + ax2)] = shift_right((placeholder[((ax0.ax1.fused*2520) + ax2)] + (int64((placeholder[0] != 0))*shift_left((int64)1, (int64(placeholder[0]) + (int64)-1)))), int64(placeholder[0]))
    }
  }
}
