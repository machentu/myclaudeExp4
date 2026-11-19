// attr [compute(tensor, 0x47c91620)] realize_scope = ""
realize tensor([0, 4], [0, 1], [0, 2520]) {
  produce tensor {
    for (ax0, 0, 4) {
      for (ax2, 0, 2520) {
        tensor(ax0, 0, ax2) =-65504.000000h
        for (k1, 0, 16) {
          tensor(ax0, 0, ax2) =max(tensor(ax0, 0, ax2), placeholder(ax0, k1, 0, ax2))
        }
      }
    }
  }
  // attr [compute(tensor, 0x483e14f0)] realize_scope = ""
  realize tensor([0, 4], [0, 1], [0, 2520]) {
    produce tensor {
      for (ax0, 0, 4) {
        for (ax2, 0, 2520) {
          tensor(ax0, 0, ax2) =0.000000h
          for (k2, 0, 16) {
            tensor(ax0, 0, ax2) =(tensor(ax0, 0, ax2) + exp((placeholder(ax0, k2, 0, ax2) - tensor(ax0, 0, ax2))))
          }
        }
      }
    }
    // attr [compute(tensor, 0x478de130)] realize_scope = ""
    realize tensor([0, 4], [0, 16], [0, 1], [0, 2520]) {
      produce tensor {
        parallel (ax0.ax1.fused, 0, 64) {
          for (ax3, 0, 2520) {
            tensor((ax0.ax1.fused/16), (ax0.ax1.fused % 16), 0, ax3) =(exp((placeholder((ax0.ax1.fused/16), (ax0.ax1.fused % 16), 0, ax3) - tensor((ax0.ax1.fused/16), 0, ax3)))/tensor((ax0.ax1.fused/16), 0, ax3))
          }
        }
      }
    }
  }
}
