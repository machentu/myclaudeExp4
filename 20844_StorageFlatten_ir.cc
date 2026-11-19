produce T_round_right_shift {
  parallel (ax0.ax1.fused, 0, 48) {
    for (ax2, 0, 64) {
      for (ax3, 0, 120) {
        T_round_right_shift[((((ax0.ax1.fused*64) + ax2)*120) + ax3)] = shift_right((placeholder[((((ax0.ax1.fused*64) + ax2)*120) + ax3)] + (int64((placeholder[(ax0.ax1.fused % 48)] != 0))*shift_left((int64)1, int64((placeholder[(ax0.ax1.fused % 48)] - 1))))), int64(placeholder[(ax0.ax1.fused % 48)]))
      }
    }
  }
}
