// attr [compute(T_concat, 0x49734740)] realize_scope = ""
realize T_concat([0, 1], [0, 56], [0, 2520]) {
  produce T_concat {
    parallel (ax0.ax1.fused, 0, 56) {
      for (ax2, 0, 2520) {
        T_concat((ax0.ax1.fused/56), (ax0.ax1.fused % 56), ax2) =tvm_if_then_else(((((ax0.ax1.fused % 56) - 4) - 1) >= 0), placeholder((ax0.ax1.fused/56), (((ax0.ax1.fused % 56) - 4) - 1), ax2), tvm_if_then_else((((ax0.ax1.fused % 56) - 4) >= 0), placeholder((ax0.ax1.fused/56), ((ax0.ax1.fused % 56) - 4), ax2), placeholder((ax0.ax1.fused/56), (ax0.ax1.fused % 56), ax2)))
      }
    }
  }
}
