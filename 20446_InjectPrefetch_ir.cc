// attr [compute(T_concat, 0x48d0c080)] realize_scope = ""
realize T_concat([0, 1], [0, 192], [0, 32], [0, 60]) {
  produce T_concat {
    parallel (ax0.ax1.fused, 0, 192) {
      for (ax2, 0, 32) {
        for (ax3, 0, 60) {
          T_concat((ax0.ax1.fused/192), (ax0.ax1.fused % 192), ax2, ax3) =tvm_if_then_else((((ax0.ax1.fused % 192) - 128) >= 0), placeholder((ax0.ax1.fused/192), ((ax0.ax1.fused % 192) - 128), ax2, ax3), placeholder((ax0.ax1.fused/192), (ax0.ax1.fused % 192), ax2, ax3))
        }
      }
    }
  }
}
