produce T_round_right_shift {
  parallel (ax0.ax1.fused, 0, 4) {
    T_round_right_shift[ax0.ax1.fused] = shift_right((placeholder[ax0.ax1.fused] + (int64((placeholder[ax0.ax1.fused] != 0))*shift_left((int64)1, (int64(placeholder[ax0.ax1.fused]) + (int64)-1)))), int64(placeholder[ax0.ax1.fused]))
  }
}
