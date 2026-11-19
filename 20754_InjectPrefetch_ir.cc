// attr [compute(nn.upsampling, 0x47105820)] realize_scope = ""
realize nn.upsampling([0, 1], [0, 256], [0, 16], [0, 30]) {
  produce nn.upsampling {
    parallel (ax0.ax1.fused, 0, 256) {
      for (ax2, 0, 16) {
        for (ax3, 0, 30) {
          nn.upsampling((ax0.ax1.fused/256), (ax0.ax1.fused % 256), ax2, ax3) =placeholder((ax0.ax1.fused/256), (ax0.ax1.fused % 256), ((ax2*8)/16), ((ax3*15)/30))
        }
      }
    }
  }
}
