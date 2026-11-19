produce T_round_right_shift {
  for (ax2, 0, 8) {
    for (ax3, 0, 15) {
      T_round_right_shift[((ax2*15) + ax3)] = shift_right((placeholder[((ax2*15) + ax3)] + (int64((placeholder[0] != 0))*shift_left((int64)1, int64((placeholder[0] - 1))))), int64(placeholder[0]))
    }
  }
}
