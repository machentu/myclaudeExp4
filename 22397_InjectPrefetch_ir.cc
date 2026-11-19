// attr [compute(T_sigmoid, 0x364cf190)] realize_scope = ""
realize T_sigmoid([0, 1], [0, 80], [0, 2520]) {
  produce T_sigmoid {
    parallel (ax0.ax1.fused, 0, 80) {
      for (ax2, 0, 2520) {
        T_sigmoid((ax0.ax1.fused/80), (ax0.ax1.fused % 80), ax2) =sigmoid(placeholder((ax0.ax1.fused/80), (ax0.ax1.fused % 80), ax2))
      }
    }
  }
}
