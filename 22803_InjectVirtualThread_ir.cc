produce T_multiply {
  for (ax2, 0, 8) {
    for (ax3, 0, 15) {
      T_multiply[((ax2*15) + ax3)] = (int64(placeholder[((ax2*15) + ax3)])*int64(placeholder[0]))
    }
  }
}
