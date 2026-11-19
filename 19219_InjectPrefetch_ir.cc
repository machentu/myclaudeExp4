// attr [compute(T_concat, 0x36264830)] realize_scope = ""
realize T_concat([0, 1], [0, 116], [0, 2520]) {
  produce T_concat {
    parallel (ax0.ax1.fused, 0, 116) {
      for (ax2, 0, 2520) {
        T_concat((ax0.ax1.fused/116), (ax0.ax1.fused % 116), ax2) =tvm_if_then_else(((((ax0.ax1.fused % 116) - 4) - 80) >= 0), placeholder((ax0.ax1.fused/116), (((ax0.ax1.fused % 116) - 4) - 80), ax2), tvm_if_then_else((((ax0.ax1.fused % 116) - 4) >= 0), placeholder((ax0.ax1.fused/116), ((ax0.ax1.fused % 116) - 4), ax2), placeholder((ax0.ax1.fused/116), (ax0.ax1.fused % 116), ax2)))
      }
    }
  }
}
