produce tensor {
  parallel (ax0.ax1.fused, 0, 128) {
    for (ax2, 0, 8) {
      for (ax3, 0, 15) {
        tensor[((((ax0.ax1.fused*8) + ax2)*15) + ax3)] = (int8)-128
        for (rv, 0, 5) {
          for (rv, 0, 5) {
            tensor[((((ax0.ax1.fused*8) + ax2)*15) + ax3)] = max(tensor[((((ax0.ax1.fused*8) + ax2)*15) + ax3)], tvm_if_then_else((((((2 - rv) <= ax2) && (ax2 < (10 - rv))) && ((2 - rv) <= ax3)) && (ax3 < (17 - rv))), placeholder[(((((((ax0.ax1.fused*8) + ax2) + rv)*15) + ax3) + rv) + -32)], (int8)-128))
          }
        }
      }
    }
  }
}
