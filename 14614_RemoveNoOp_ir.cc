produce T_round_right_shift {
  parallel (ax0.ax1.fused, 0, 256) {
    for (ax2, 0, 32) {
      for (ax3, 0, 16) {
        T_round_right_shift[((((ax0.ax1.fused*32) + ax2)*16) + ax3)] = shift_right((placeholder[((((ax0.ax1.fused*32) + ax2)*16) + ax3)] + (int64((placeholder[0] != 0))*shift_left((int64)1, (int64(placeholder[0]) + (int64)-1)))), int64(placeholder[0]))
      }
    }
  }
}
