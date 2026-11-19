// attr [compute(T_round_right_shift, 0x3eaf5db0)] realize_scope = ""
realize T_round_right_shift([0, 1], [0, 384], [0, 8], [0, 15]) {
  produce T_round_right_shift {
    parallel (ax0.ax1.fused, 0, 384) {
      for (ax2, 0, 8) {
        for (ax3, 0, 15) {
          T_round_right_shift((ax0.ax1.fused/384), (ax0.ax1.fused % 384), ax2, ax3) =shift_right((placeholder((ax0.ax1.fused/384), (ax0.ax1.fused % 384), ax2, ax3) + (int64((placeholder((ax0.ax1.fused % 384), 0, 0) != 0))*shift_left((int64)1, int64((placeholder((ax0.ax1.fused % 384), 0, 0) - 1))))), int64(placeholder((ax0.ax1.fused % 384), 0, 0)))
        }
      }
    }
  }
}
