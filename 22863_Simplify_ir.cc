produce T_sigmoid {
  for (ax2, 0, 2520) {
    T_sigmoid[ax2] = sigmoid(placeholder[ax2])
  }
}
