produce T_reshape {
  for (ax2, 0, 512) {
    T_reshape[ax2] = placeholder[(ax2 % 512)]
  }
}
