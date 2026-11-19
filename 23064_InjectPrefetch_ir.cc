// attr [compute(T_concat, 0x4561e0f0)] realize_scope = ""
realize T_concat([0, 1], [0, 51], [0, 2520]) {
  produce T_concat {
    parallel (ax0.ax1.fused, 0, 51) {
      for (ax2, 0, 2520) {
        T_concat((ax0.ax1.fused/51), (ax0.ax1.fused % 51), ax2) =tvm_if_then_else((((ax2 - 1920) - 480) >= 0), placeholder((ax0.ax1.fused/51), (ax0.ax1.fused % 51), ((ax2 - 1920) - 480)), tvm_if_then_else(((ax2 - 1920) >= 0), placeholder((ax0.ax1.fused/51), (ax0.ax1.fused % 51), (ax2 - 1920)), placeholder((ax0.ax1.fused/51), (ax0.ax1.fused % 51), ax2)))
      }
    }
  }
}
