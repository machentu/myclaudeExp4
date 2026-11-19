produce T_round_right_shift {
  parallel (ax0.ax1.fused, 0, 65) {
    for (ax2, 0, 1920) {
      T_round_right_shift[((ax0.ax1.fused*1920) + ax2)] = int32(shift_right((int64(placeholder[((ax0.ax1.fused*1920) + ax2)]) + (int64((placeholder[0] != 0))*shift_left((int64)1, int64((placeholder[0] - 1))))), int64(placeholder[0])))
    }
  }
}
