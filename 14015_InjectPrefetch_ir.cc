// attr [compute(T_round_right_shift, 0x2e714120)] realize_scope = ""
realize T_round_right_shift([0, 1], [0, 384], [0, 16], [0, 8]) {
  produce T_round_right_shift {
    parallel (ax0.ax1.fused, 0, 384) {
      for (ax2, 0, 16) {
        for (ax3, 0, 8) {
          T_round_right_shift((ax0.ax1.fused/384), (ax0.ax1.fused % 384), ax2, ax3) =shift_right((placeholder((ax0.ax1.fused/384), (ax0.ax1.fused % 384), ax2, ax3) + (int64((placeholder() != 0))*shift_left((int64)1, int64((placeholder() - 1))))), int64(placeholder()))
        }
      }
    }
  }
}
