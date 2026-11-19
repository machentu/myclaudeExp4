// attr [compute(T_round_right_shift, 0x3a8f7ea0)] realize_scope = ""
realize T_round_right_shift([0, 1], [0, 192], [0, 16], [0, 30]) {
  produce T_round_right_shift {
    parallel (ax0.ax1.fused, 0, 192) {
      for (ax2, 0, 16) {
        for (ax3, 0, 30) {
          T_round_right_shift((ax0.ax1.fused/192), (ax0.ax1.fused % 192), ax2, ax3) =shift_right((placeholder((ax0.ax1.fused/192), (ax0.ax1.fused % 192), ax2, ax3) + (int64((placeholder((ax0.ax1.fused % 192), 0, 0) != 0))*shift_left((int64)1, int64((placeholder((ax0.ax1.fused % 192), 0, 0) - 1))))), int64(placeholder((ax0.ax1.fused % 192), 0, 0)))
        }
      }
    }
  }
}
