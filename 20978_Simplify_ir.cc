produce T_round_right_shift {
  parallel (ax0.ax1.fused, 0, 16) {
    for (ax2, 0, 128) {
      for (ax3, 0, 240) {
        T_round_right_shift[((((ax0.ax1.fused*128) + ax2)*240) + ax3)] = shift_right((placeholder[((((ax0.ax1.fused*128) + ax2)*240) + ax3)] + (int64((placeholder[ax0.ax1.fused] != 0))*shift_left((int64)1, (int64(placeholder[ax0.ax1.fused]) + (int64)-1)))), int64(placeholder[ax0.ax1.fused]))
      }
    }
  }
}
