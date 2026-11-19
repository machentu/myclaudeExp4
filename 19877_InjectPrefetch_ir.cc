// attr [compute(T_relu, 0x36511f30)] realize_scope = ""
realize T_relu([0, 1], [0, 128], [0, 16], [0, 30]) {
  produce T_relu {
    parallel (ax0.ax1.fused, 0, 128) {
      for (ax2, 0, 16) {
        for (ax3, 0, 30) {
          T_relu((ax0.ax1.fused/128), (ax0.ax1.fused % 128), ax2, ax3) =max(placeholder((ax0.ax1.fused/128), (ax0.ax1.fused % 128), ax2, ax3), (int8)0)
        }
      }
    }
  }
}
