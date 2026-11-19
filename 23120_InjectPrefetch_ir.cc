// attr [compute(T_reshape, 0x47e1abc0)] realize_scope = ""
realize T_reshape([0, 1], [0, 51], [0, 1920]) {
  produce T_reshape {
    parallel (ax0.ax1.fused, 0, 51) {
      for (ax2, 0, 1920) {
        T_reshape((ax0.ax1.fused/51), (ax0.ax1.fused % 51), ax2) =placeholder(0, ((((((((ax0.ax1.fused/51)*51) + (ax0.ax1.fused % 51))*1920) + ax2)/60)/32) % 51), (((((((ax0.ax1.fused/51)*51) + (ax0.ax1.fused % 51))*1920) + ax2)/60) % 32), ((((((ax0.ax1.fused/51)*51) + (ax0.ax1.fused % 51))*1920) + ax2) % 60))
      }
    }
  }
}
