// attr [compute(T_round_right_shift, 0x3c2777e0)] realize_scope = ""
realize T_round_right_shift([0, 1], [0, 144], [0, 2520]) {
  produce T_round_right_shift {
    parallel (ax0.ax1.fused, 0, 144) {
      for (ax2, 0, 2520) {
        T_round_right_shift((ax0.ax1.fused/144), (ax0.ax1.fused % 144), ax2) =shift_right((placeholder((ax0.ax1.fused/144), (ax0.ax1.fused % 144), ax2) + (int64((placeholder() != 0))*shift_left((int64)1, int64((placeholder() - 1))))), int64(placeholder()))
      }
    }
  }
}
