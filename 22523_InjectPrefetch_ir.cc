// attr [compute(T_concat, 0x373ba870)] realize_scope = ""
realize T_concat([0, 1], [0, 32], [0, 2520]) {
  produce T_concat {
    parallel (ax0.ax1.fused, 0, 32) {
      for (ax2, 0, 2520) {
        T_concat((ax0.ax1.fused/32), (ax0.ax1.fused % 32), ax2) =tvm_if_then_else((((ax2 - 1920) - 480) >= 0), placeholder((ax0.ax1.fused/32), (ax0.ax1.fused % 32), ((ax2 - 1920) - 480)), tvm_if_then_else(((ax2 - 1920) >= 0), placeholder((ax0.ax1.fused/32), (ax0.ax1.fused % 32), (ax2 - 1920)), placeholder((ax0.ax1.fused/32), (ax0.ax1.fused % 32), ax2)))
      }
    }
  }
}
