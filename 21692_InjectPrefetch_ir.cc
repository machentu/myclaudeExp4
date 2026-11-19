// attr [compute(T_round_right_shift, 0x456280e0)] realize_scope = ""
realize T_round_right_shift([0, 1], [0, 16], [0, 64], [0, 120]) {
  produce T_round_right_shift {
    parallel (ax0.ax1.fused, 0, 16) {
      for (ax2, 0, 64) {
        for (ax3, 0, 120) {
          T_round_right_shift((ax0.ax1.fused/16), (ax0.ax1.fused % 16), ax2, ax3) =shift_right((placeholder((ax0.ax1.fused/16), (ax0.ax1.fused % 16), ax2, ax3) + (int64((placeholder((ax0.ax1.fused % 16), 0, 0) != 0))*shift_left((int64)1, int64((placeholder((ax0.ax1.fused % 16), 0, 0) - 1))))), int64(placeholder((ax0.ax1.fused % 16), 0, 0)))
        }
      }
    }
  }
}
