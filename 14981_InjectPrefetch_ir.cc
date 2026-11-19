// attr [compute(tensor, 0x22911bf0)] realize_scope = ""
realize tensor([0, 1], [0, 64], [0, 32], [0, 16]) {
  produce tensor {
    parallel (ax0.ax1.fused, 0, 64) {
      for (ax2, 0, 32) {
        for (ax3, 0, 16) {
          tensor((ax0.ax1.fused/64), (ax0.ax1.fused % 64), ax2, ax3) =(int8)-128
          for (rv, 0, 3) {
            for (rv, 0, 3) {
              tensor((ax0.ax1.fused/64), (ax0.ax1.fused % 64), ax2, ax3) =max(tensor((ax0.ax1.fused/64), (ax0.ax1.fused % 64), ax2, ax3), tvm_if_then_else(((((((ax2*2) + rv) >= 1) && (((ax2*2) + rv) < 65)) && (((ax3*2) + rv) >= 1)) && (((ax3*2) + rv) < 33)), placeholder((ax0.ax1.fused/64), (ax0.ax1.fused % 64), (((ax2*2) + rv) - 1), (((ax3*2) + rv) - 1)), (int8)-128))
            }
          }
        }
      }
    }
  }
}
