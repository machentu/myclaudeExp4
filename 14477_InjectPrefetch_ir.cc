// attr [compute(tensor, 0x2e48c2a0)] realize_scope = ""
realize tensor([0, 1], [0, 256], [0, 16], [0, 8]) {
  produce tensor {
    parallel (ax0.ax1.fused, 0, 256) {
      for (ax2, 0, 16) {
        for (ax3, 0, 8) {
          tensor((ax0.ax1.fused/256), (ax0.ax1.fused % 256), ax2, ax3) =0
          for (rv, 0, 2) {
            for (rv, 0, 2) {
              tensor((ax0.ax1.fused/256), (ax0.ax1.fused % 256), ax2, ax3) =(tensor((ax0.ax1.fused/256), (ax0.ax1.fused % 256), ax2, ax3) + int32(placeholder((ax0.ax1.fused/256), (ax0.ax1.fused % 256), ((ax2*2) + rv), ((ax3*2) + rv))))
            }
          }
        }
      }
    }
  }
}
