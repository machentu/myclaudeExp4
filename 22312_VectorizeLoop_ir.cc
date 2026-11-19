produce T_multiply {
  for (ax2, 0, 32) {
    for (ax3, 0, 60) {
      T_multiply[((ax2*60) + ax3)] = (int64(placeholder[((ax2*60) + ax3)])*int64(placeholder[0]))
    }
  }
}
