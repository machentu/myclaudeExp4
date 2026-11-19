produce T_add {
  for (ax2, 0, 8) {
    for (ax3, 0, 15) {
      T_add[((ax2*15) + ax3)] = int64((placeholder[((ax2*15) + ax3)] + placeholder[0]))
    }
  }
}
