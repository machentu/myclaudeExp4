// attr [compute(T_sigmoid, 0x4797b9d0)] realize_scope = ""
realize T_sigmoid([0, 1], [0, 17], [0, 1], [0, 2520]) {
  produce T_sigmoid {
    parallel (ax0.ax1.fused, 0, 17) {
      for (ax3, 0, 2520) {
        T_sigmoid((ax0.ax1.fused/17), (ax0.ax1.fused % 17), 0, ax3) =sigmoid(placeholder((ax0.ax1.fused/17), (ax0.ax1.fused % 17), 0, ax3))
      }
    }
  }
}
