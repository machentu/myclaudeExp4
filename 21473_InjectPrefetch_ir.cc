// attr [compute(T_round_right_shift, 0x38fb1a90)] realize_scope = ""
realize T_round_right_shift([0, 1], [0, 64], [0, 16], [0, 30]) {
  produce T_round_right_shift {
    parallel (ax0.ax1.fused, 0, 64) {
      for (ax2, 0, 16) {
        for (ax3, 0, 30) {
          T_round_right_shift((ax0.ax1.fused/64), (ax0.ax1.fused % 64), ax2, ax3) =int32(shift_right((int64(placeholder((ax0.ax1.fused/64), (ax0.ax1.fused % 64), ax2, ax3)) + (int64((placeholder((ax0.ax1.fused % 64), 0, 0) != 0))*shift_left((int64)1, int64((placeholder((ax0.ax1.fused % 64), 0, 0) - 1))))), int64(placeholder((ax0.ax1.fused % 64), 0, 0))))
        }
      }
    }
  }
}
