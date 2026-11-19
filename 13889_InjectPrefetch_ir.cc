// attr [compute(tensor, 0x2375d030)] realize_scope = ""
realize tensor([0, 1], [0, 384], [0, 8], [0, 4]) {
  produce tensor {
    parallel (ax0.ax1.fused, 0, 384) {
      for (ax2, 0, 8) {
        for (ax3, 0, 4) {
          tensor((ax0.ax1.fused/384), (ax0.ax1.fused % 384), ax2, ax3) =0
          for (rv, 0, 2) {
            for (rv, 0, 2) {
              tensor((ax0.ax1.fused/384), (ax0.ax1.fused % 384), ax2, ax3) =(tensor((ax0.ax1.fused/384), (ax0.ax1.fused % 384), ax2, ax3) + int32(placeholder((ax0.ax1.fused/384), (ax0.ax1.fused % 384), ((ax2*2) + rv), ((ax3*2) + rv))))
            }
          }
        }
      }
    }
  }
}
