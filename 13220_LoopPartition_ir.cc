produce T_add {
  parallel (ax0.ax1.fused, 0, 512) {
    T_add[ax0.ax1.fused] = int64((placeholder[ax0.ax1.fused] + placeholder[ax0.ax1.fused]))
  }
}
