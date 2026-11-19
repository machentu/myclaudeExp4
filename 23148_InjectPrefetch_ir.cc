// attr [compute(T_round_right_shift, 0x461f84e0)] realize_scope = ""
realize T_round_right_shift([0, 1], [0, 51], [0, 32], [0, 60]) {
  produce T_round_right_shift {
    parallel (ax0.ax1.fused, 0, 51) {
      for (ax2, 0, 32) {
        for (ax3, 0, 60) {
          T_round_right_shift((ax0.ax1.fused/51), (ax0.ax1.fused % 51), ax2, ax3) =shift_right((placeholder((ax0.ax1.fused/51), (ax0.ax1.fused % 51), ax2, ax3) + (int64((placeholder((ax0.ax1.fused % 51), 0, 0) != 0))*shift_left((int64)1, int64((placeholder((ax0.ax1.fused % 51), 0, 0) - 1))))), int64(placeholder((ax0.ax1.fused % 51), 0, 0)))
        }
      }
    }
  }
}
