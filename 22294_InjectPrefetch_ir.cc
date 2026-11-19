// attr [compute(T_round_right_shift, 0x3cae4260)] realize_scope = ""
realize T_round_right_shift([0, 1], [0, 1], [0, 32], [0, 60]) {
  produce T_round_right_shift {
    for (ax2, 0, 32) {
      for (ax3, 0, 60) {
        T_round_right_shift(0, 0, ax2, ax3) =shift_right((placeholder(0, 0, ax2, ax3) + (int64((placeholder() != 0))*shift_left((int64)1, int64((placeholder() - 1))))), int64(placeholder()))
      }
    }
  }
}
