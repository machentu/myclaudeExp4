// attr [compute(T_round_right_shift, 0x49dd1280)] realize_scope = ""
realize T_round_right_shift([0, 1], [0, 1], [0, 16], [0, 30]) {
  produce T_round_right_shift {
    for (ax2, 0, 16) {
      for (ax3, 0, 30) {
        T_round_right_shift(0, 0, ax2, ax3) =shift_right((placeholder(0, 0, ax2, ax3) + (int64((placeholder() != 0))*shift_left((int64)1, int64((placeholder() - 1))))), int64(placeholder()))
      }
    }
  }
}
