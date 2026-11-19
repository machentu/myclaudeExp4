// attr [tensor] storage_scope = "global"
allocate tensor[float16 * 4 * 1 * 2520]
produce tensor {
  for (ax0, 0, 4) {
    for (ax2, 0, 2520) {
      tensor[((ax0*2520) + ax2)] = -65504.000000h
      for (k1, 0, 16) {
        tensor[((ax0*2520) + ax2)] = max(tensor[((ax0*2520) + ax2)], placeholder[(((ax0*40320) + (k1*2520)) + ax2)])
      }
    }
  }
}
// attr [tensor] storage_scope = "global"
allocate tensor[float16 * 4 * 1 * 2520]
produce tensor {
  for (ax0, 0, 4) {
    for (ax2, 0, 2520) {
      tensor[((ax0*2520) + ax2)] = 0.000000h
      for (k2, 0, 16) {
        tensor[((ax0*2520) + ax2)] = (tensor[((ax0*2520) + ax2)] + exp((placeholder[(((ax0*40320) + (k2*2520)) + ax2)] - tensor[((ax0*2520) + ax2)])))
      }
    }
  }
}
produce tensor {
  parallel (ax0.ax1.fused, 0, 64) {
    for (ax3, 0, 2520) {
      tensor[((ax0.ax1.fused*2520) + ax3)] = (exp((placeholder[((ax0.ax1.fused*2520) + ax3)] - tensor[(((ax0.ax1.fused/16)*2520) + ax3)]))/tensor[(((ax0.ax1.fused/16)*2520) + ax3)])
    }
  }
}
