// attr [compute(tensor, 0x1feca480)] realize_scope = ""
realize tensor([0, 1], [0, 64], [0, 1], [0, 1]) {
  produce tensor {
    parallel (ax0.ax1.fused, 0, 64) {
      tensor((ax0.ax1.fused/64), (ax0.ax1.fused % 64), 0, 0) =0
      for (rv, 0, 32) {
        for (rv, 0, 16) {
          tensor((ax0.ax1.fused/64), (ax0.ax1.fused % 64), 0, 0) =(tensor((ax0.ax1.fused/64), (ax0.ax1.fused % 64), 0, 0) + int32(placeholder((ax0.ax1.fused/64), (ax0.ax1.fused % 64), (0 + rv), (0 + rv))))
        }
      }
    }
  }
}
