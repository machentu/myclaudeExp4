produce nn.upsampling {
  parallel (ax0.ax1.fused, 0, 256) {
    for (ax2, 0, 16) {
      for (ax3, 0, 30) {
        nn.upsampling[(((ax0.ax1.fused*480) + (ax2*30)) + ax3)] = placeholder[(((ax0.ax1.fused*120) + ((ax2/2)*15)) + (ax3/2))]
      }
    }
  }
}
