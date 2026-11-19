produce T_add {
  for (ax2, 0, 32) {
    for (ax3, 0, 60) {
      T_add[((ax2*60) + ax3)] = int64((placeholder[((ax2*60) + ax3)] + placeholder[0]))
    }
  }
}
