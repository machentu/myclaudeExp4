produce T_cast {
  for (ax2, 0, 2520) {
    T_cast[ax2] = float16(placeholder[ax2])
  }
}
