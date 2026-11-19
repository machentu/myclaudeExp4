// attr [compute(T_concat, 0x43a7f340)] realize_scope = ""
realize T_concat([0, 1], [0, 384], [0, 8], [0, 15]) {
  produce T_concat {
    parallel (ax0.ax1.fused, 0, 384) {
      for (ax2, 0, 8) {
        for (ax3, 0, 15) {
          T_concat((ax0.ax1.fused/384), (ax0.ax1.fused % 384), ax2, ax3) =tvm_if_then_else((((ax0.ax1.fused % 384) - 128) >= 0), placeholder((ax0.ax1.fused/384), ((ax0.ax1.fused % 384) - 128), ax2, ax3), placeholder((ax0.ax1.fused/384), (ax0.ax1.fused % 384), ax2, ax3))
        }
      }
    }
  }
}
