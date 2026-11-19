// attr [compute(T_round_right_shift, 0x29bb0040)] realize_scope = ""
realize T_round_right_shift([0, 1], [0, 128], [0, 1], [0, 1]) {
  produce T_round_right_shift {
    parallel (ax0.ax1.fused, 0, 128) {
      T_round_right_shift((ax0.ax1.fused/128), (ax0.ax1.fused % 128), 0, 0) =shift_right((placeholder((ax0.ax1.fused/128), (ax0.ax1.fused % 128), 0, 0) + (int64((placeholder((ax0.ax1.fused % 128), 0, 0) != 0))*shift_left((int64)1, int64((placeholder((ax0.ax1.fused % 128), 0, 0) - 1))))), int64(placeholder((ax0.ax1.fused % 128), 0, 0)))
    }
  }
}
