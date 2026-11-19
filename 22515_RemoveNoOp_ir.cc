produce T_round_right_shift {
  for (ax2, 0, 16) {
    for (ax3, 0, 30) {
      T_round_right_shift[((ax2*30) + ax3)] = shift_right((placeholder[((ax2*30) + ax3)] + (int64((placeholder[0] != 0))*shift_left((int64)1, (int64(placeholder[0]) + (int64)-1)))), int64(placeholder[0]))
    }
  }
}
