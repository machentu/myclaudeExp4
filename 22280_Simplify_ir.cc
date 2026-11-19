produce T_round_right_shift {
  parallel (ax0.ax1.fused, 0, 80) {
    for (ax2, 0, 8) {
      for (ax3, 0, 15) {
        T_round_right_shift[((((ax0.ax1.fused*8) + ax2)*15) + ax3)] = shift_right((placeholder[((((ax0.ax1.fused*8) + ax2)*15) + ax3)] + (int64((placeholder[ax0.ax1.fused] != 0))*shift_left((int64)1, (int64(placeholder[ax0.ax1.fused]) + (int64)-1)))), int64(placeholder[ax0.ax1.fused]))
      }
    }
  }
}
