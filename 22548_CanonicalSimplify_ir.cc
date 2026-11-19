produce T_add {
  for (ax2, 0, 16) {
    for (ax3, 0, 30) {
      T_add[((ax2*30) + ax3)] = int64((placeholder[((ax2*30) + ax3)] + placeholder[0]))
    }
  }
}
