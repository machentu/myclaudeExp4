// attr [compute(T_multiply, 0x46fc4730)] realize_scope = ""
realize T_multiply([0, 1], [0, 51], [0, 480]) {
  produce T_multiply {
    parallel (ax0.ax1.fused, 0, 51) {
      for (ax2, 0, 480) {
        T_multiply((ax0.ax1.fused/51), (ax0.ax1.fused % 51), ax2) =(int32(placeholder((ax0.ax1.fused/51), (ax0.ax1.fused % 51), ax2))*int32(placeholder()))
      }
    }
  }
}
