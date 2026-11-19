produce T_cast {
  for (ax1, 0, 512) {
    T_cast[ax1] = float16(placeholder[ax1])
  }
}
