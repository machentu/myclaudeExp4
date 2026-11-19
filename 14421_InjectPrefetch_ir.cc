// attr [compute(T_round_right_shift, 0x2e182a20)] realize_scope = ""
realize T_round_right_shift([0, 1], [0, 256], [0, 16], [0, 8]) {
  produce T_round_right_shift {
    parallel (ax0.ax1.fused, 0, 256) {
      for (ax2, 0, 16) {
        for (ax3, 0, 8) {
          T_round_right_shift((ax0.ax1.fused/256), (ax0.ax1.fused % 256), ax2, ax3) =shift_right((placeholder((ax0.ax1.fused/256), (ax0.ax1.fused % 256), ax2, ax3) + (int64((placeholder((ax0.ax1.fused % 256), 0, 0) != 0))*shift_left((int64)1, int64((placeholder((ax0.ax1.fused % 256), 0, 0) - 1))))), int64(placeholder((ax0.ax1.fused % 256), 0, 0)))
        }
      }
    }
  }
}
