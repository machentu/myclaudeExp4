// attr [compute(nn.upsampling, 0x3cf5a380)] realize_scope = ""
realize nn.upsampling([0, 1], [0, 128], [0, 32], [0, 60]) {
  produce nn.upsampling {
    parallel (ax0.ax1.fused, 0, 128) {
      for (ax2, 0, 32) {
        for (ax3, 0, 60) {
          nn.upsampling((ax0.ax1.fused/128), (ax0.ax1.fused % 128), ax2, ax3) =placeholder((ax0.ax1.fused/128), (ax0.ax1.fused % 128), ((ax2*16)/32), ((ax3*30)/60))
        }
      }
    }
  }
}
