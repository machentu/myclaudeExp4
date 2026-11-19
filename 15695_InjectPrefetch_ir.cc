// attr [compute(tensor, 0x1b09ad40)] realize_scope = ""
realize tensor([0, 1], [0, 96], [0, 1], [0, 1]) {
  produce tensor {
    parallel (ax0.ax1.fused, 0, 96) {
      tensor((ax0.ax1.fused/96), (ax0.ax1.fused % 96), 0, 0) =0
      for (rv, 0, 16) {
        for (rv, 0, 8) {
          tensor((ax0.ax1.fused/96), (ax0.ax1.fused % 96), 0, 0) =(tensor((ax0.ax1.fused/96), (ax0.ax1.fused % 96), 0, 0) + int32(placeholder((ax0.ax1.fused/96), (ax0.ax1.fused % 96), (0 + rv), (0 + rv))))
        }
      }
    }
  }
}
