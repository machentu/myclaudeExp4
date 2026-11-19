// attr [compute(T_round_right_shift, 0x48810b20)] realize_scope = ""
realize T_round_right_shift([0, 1], [0, 1], [0, 8], [0, 15]) {
  produce T_round_right_shift {
    for (ax2, 0, 8) {
      for (ax3, 0, 15) {
        T_round_right_shift(0, 0, ax2, ax3) =shift_right((placeholder(0, 0, ax2, ax3) + (int64((placeholder() != 0))*shift_left((int64)1, int64((placeholder() - 1))))), int64(placeholder()))
      }
    }
  }
}
