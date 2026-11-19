produce T_relu {
  for (ax1, 0, 512) {
    T_relu[ax1] = max(placeholder[ax1], 0.000000h)
  }
}
