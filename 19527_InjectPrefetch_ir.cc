// attr [compute(T_reshape, 0x3604b590)] realize_scope = ""
realize T_reshape([0, 1], [0, 144], [0, 1920]) {
  produce T_reshape {
    parallel (ax0.ax1.fused, 0, 144) {
      for (ax2, 0, 1920) {
        T_reshape((ax0.ax1.fused/144), (ax0.ax1.fused % 144), ax2) =placeholder(0, ((((((((ax0.ax1.fused/144)*144) + (ax0.ax1.fused % 144))*1920) + ax2)/60)/32) % 144), (((((((ax0.ax1.fused/144)*144) + (ax0.ax1.fused % 144))*1920) + ax2)/60) % 32), ((((((ax0.ax1.fused/144)*144) + (ax0.ax1.fused % 144))*1920) + ax2) % 60))
      }
    }
  }
}
