produce T_cast {
  for (ax1, 0, 512) {
    T_cast[ax1] = float32(placeholder[ax1])
  }
}
