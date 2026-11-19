// attr [compute(T_round_right_shift, 0x36390800)] realize_scope = ""
realize T_round_right_shift([0, 1], [0, 32], [0, 120]) {
  produce T_round_right_shift {
    parallel (ax0.ax1.fused, 0, 32) {
      for (ax2, 0, 120) {
        T_round_right_shift((ax0.ax1.fused/32), (ax0.ax1.fused % 32), ax2) =int32(shift_right((int64(placeholder((ax0.ax1.fused/32), (ax0.ax1.fused % 32), ax2)) + (int64((placeholder() != 0))*shift_left((int64)1, int64((placeholder() - 1))))), int64(placeholder())))
      }
    }
  }
}
