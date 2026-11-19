// attr [compute(T_round_right_shift, 0x2df7e540)] realize_scope = ""
realize T_round_right_shift([0, 1], [0, 256], [0, 32], [0, 16]) {
  produce T_round_right_shift {
    parallel (ax0.ax1.fused, 0, 256) {
      for (ax2, 0, 32) {
        for (ax3, 0, 16) {
          T_round_right_shift((ax0.ax1.fused/256), (ax0.ax1.fused % 256), ax2, ax3) =shift_right((placeholder((ax0.ax1.fused/256), (ax0.ax1.fused % 256), ax2, ax3) + (int64((placeholder() != 0))*shift_left((int64)1, int64((placeholder() - 1))))), int64(placeholder()))
        }
      }
    }
  }
}
