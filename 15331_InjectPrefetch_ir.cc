// attr [compute(T_round_right_shift, 0x2ea0de30)] realize_scope = ""
realize T_round_right_shift([0, 1], [0, 4], [0, 1], [0, 1]) {
  produce T_round_right_shift {
    parallel (ax0.ax1.fused, 0, 4) {
      T_round_right_shift((ax0.ax1.fused/4), (ax0.ax1.fused % 4), 0, 0) =shift_right((placeholder((ax0.ax1.fused/4), (ax0.ax1.fused % 4), 0, 0) + (int64((placeholder((ax0.ax1.fused % 4), 0, 0) != 0))*shift_left((int64)1, int64((placeholder((ax0.ax1.fused % 4), 0, 0) - 1))))), int64(placeholder((ax0.ax1.fused % 4), 0, 0)))
    }
  }
}
