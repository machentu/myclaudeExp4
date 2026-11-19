produce T_multiply {
  for (ax1, 0, 512) {
    T_multiply[ax1] = (placeholder[ax1]*placeholder[ax1])
  }
}
