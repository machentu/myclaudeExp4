// attr [compute(T_reshape, 0x488a5780)] realize_scope = ""
realize T_reshape([0, 1], [0, 65], [0, 1920]) {
  produce T_reshape {
    parallel (ax0.ax1.fused, 0, 65) {
      for (ax2, 0, 1920) {
        T_reshape((ax0.ax1.fused/65), (ax0.ax1.fused % 65), ax2) =placeholder(0, ((((((((ax0.ax1.fused/65)*65) + (ax0.ax1.fused % 65))*1920) + ax2)/60)/32) % 65), (((((((ax0.ax1.fused/65)*65) + (ax0.ax1.fused % 65))*1920) + ax2)/60) % 32), ((((((ax0.ax1.fused/65)*65) + (ax0.ax1.fused % 65))*1920) + ax2) % 60))
      }
    }
  }
}
