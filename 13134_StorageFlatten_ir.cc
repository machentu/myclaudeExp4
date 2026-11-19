produce T_reshape {
  for (ax1, 0, 512) {
    T_reshape[ax1] = placeholder[(ax1 % 512)]
  }
}
