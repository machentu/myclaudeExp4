// attr [compute(T_concat, 0x4dcd5300)] realize_scope = ""
realize T_concat([0, 1], [0, 128], [0, 32], [0, 60]) {
  produce T_concat {
    parallel (ax0.ax1.fused, 0, 128) {
      for (ax2, 0, 32) {
        for (ax3, 0, 60) {
          T_concat((ax0.ax1.fused/128), (ax0.ax1.fused % 128), ax2, ax3) =tvm_if_then_else((((((ax0.ax1.fused % 128) - 32) - 32) - 32) >= 0), placeholder((ax0.ax1.fused/128), ((((ax0.ax1.fused % 128) - 32) - 32) - 32), ax2, ax3), tvm_if_then_else(((((ax0.ax1.fused % 128) - 32) - 32) >= 0), placeholder((ax0.ax1.fused/128), (((ax0.ax1.fused % 128) - 32) - 32), ax2, ax3), tvm_if_then_else((((ax0.ax1.fused % 128) - 32) >= 0), placeholder((ax0.ax1.fused/128), ((ax0.ax1.fused % 128) - 32), ax2, ax3), placeholder((ax0.ax1.fused/128), (ax0.ax1.fused % 128), ax2, ax3))))
        }
      }
    }
  }
}
