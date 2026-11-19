produce nn.upsampling {
  parallel (ax0.ax1.fused, 0, 128) {
    for (ax2, 0, 32) {
      for (ax3, 0, 60) {
        nn.upsampling[(((ax0.ax1.fused*1920) + (ax2*60)) + ax3)] = placeholder[(((ax0.ax1.fused*480) + ((ax2/2)*30)) + (ax3/2))]
      }
    }
  }
}
