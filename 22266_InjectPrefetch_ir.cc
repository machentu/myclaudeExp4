// attr [compute(tensor, 0x49dcd120)] realize_scope = ""
realize tensor([0, 1], [0, 128], [0, 8], [0, 15]) {
  produce tensor {
    parallel (ax0.ax1.fused, 0, 128) {
      for (ax2, 0, 8) {
        for (ax3, 0, 15) {
          tensor((ax0.ax1.fused/128), (ax0.ax1.fused % 128), ax2, ax3) =(int8)-128
          for (rv, 0, 5) {
            for (rv, 0, 5) {
              tensor((ax0.ax1.fused/128), (ax0.ax1.fused % 128), ax2, ax3) =max(tensor((ax0.ax1.fused/128), (ax0.ax1.fused % 128), ax2, ax3), tvm_if_then_else((((((ax2 + rv) >= 2) && ((ax2 + rv) < 10)) && ((ax3 + rv) >= 2)) && ((ax3 + rv) < 17)), placeholder((ax0.ax1.fused/128), (ax0.ax1.fused % 128), ((ax2 + rv) - 2), ((ax3 + rv) - 2)), (int8)-128))
            }
          }
        }
      }
    }
  }
}
