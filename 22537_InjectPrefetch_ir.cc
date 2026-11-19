// attr [compute(T_reshape, 0x373aa810)] realize_scope = ""
realize T_reshape([0, 1], [0, 32], [0, 1920]) {
  produce T_reshape {
    parallel (ax0.ax1.fused, 0, 32) {
      for (ax2, 0, 1920) {
        T_reshape((ax0.ax1.fused/32), (ax0.ax1.fused % 32), ax2) =placeholder(0, ((((((((ax0.ax1.fused/32)*32) + (ax0.ax1.fused % 32))*1920) + ax2)/60)/32) % 32), (((((((ax0.ax1.fused/32)*32) + (ax0.ax1.fused % 32))*1920) + ax2)/60) % 32), ((((((ax0.ax1.fused/32)*32) + (ax0.ax1.fused % 32))*1920) + ax2) % 60))
      }
    }
  }
}
