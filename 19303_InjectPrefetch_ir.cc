// attr [compute(T_round_right_shift, 0x365732b0)] realize_scope = ""
realize T_round_right_shift([0, 4], [0, 1], [0, 1], [0, 2520]) {
  produce T_round_right_shift {
    parallel (ax0.ax1.fused, 0, 4) {
      for (ax3, 0, 2520) {
        T_round_right_shift(ax0.ax1.fused, 0, 0, ax3) =shift_right((placeholder(ax0.ax1.fused, 0, 0, ax3) + (int64((placeholder() != 0))*shift_left((int64)1, int64((placeholder() - 1))))), int64(placeholder()))
      }
    }
  }
}
