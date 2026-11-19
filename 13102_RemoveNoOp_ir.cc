produce T_add {
  for (ax1, 0, 512) {
    T_add[ax1] = (placeholder[ax1] + placeholder[ax1])
  }
}
