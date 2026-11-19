// attr [compute(T_cast, 0x227d18f0)] realize_scope = ""
realize T_cast([0, 1], [0, 384], [0, 16], [0, 8]) {
  produce T_cast {
    for (ax1, 0, 384) {
      for (ax2, 0, 16) {
        for (ax3, 0, 8) {
          T_cast(0, ax1, ax2, ax3) =float32(placeholder(0, ax1, ax2, ax3))
        }
      }
    }
  }
  // attr [compute(T_divide, 0x26dd8930)] realize_scope = ""
  realize T_divide([0, 1], [0, 384], [0, 16], [0, 8]) {
    produce T_divide {
      for (ax1, 0, 384) {
        for (ax2, 0, 16) {
          for (ax3, 0, 8) {
            T_divide(0, ax1, ax2, ax3) =(T_cast(0, ax1, ax2, ax3)/128.000000f)
          }
        }
      }
    }
    // attr [compute(T_sum, 0x2375cb20)] realize_scope = ""
    realize T_sum([0, 1], [0, 384], [0, 1], [0, 1]) {
      produce T_sum {
        for (ax1, 0, 384) {
          T_sum(0, ax1, 0, 0) =0.000000f
          for (k2, 0, 16) {
            for (k3, 0, 8) {
              T_sum(0, ax1, 0, 0) =(T_sum(0, ax1, 0, 0) + T_divide(0, ax1, k2, k3))
            }
          }
        }
      }
      // attr [compute(T_subtract, 0x2b30da10)] realize_scope = ""
      realize T_subtract([0, 1], [0, 384], [0, 16], [0, 8]) {
        produce T_subtract {
          for (ax1, 0, 384) {
            for (ax2, 0, 16) {
              for (ax3, 0, 8) {
                T_subtract(0, ax1, ax2, ax3) =(T_cast(0, ax1, ax2, ax3) - T_sum(0, ax1, 0, 0))
              }
            }
          }
        }
        // attr [compute(T_power, 0x190977b0)] realize_scope = ""
        realize T_power([0, 1], [0, 384], [0, 16], [0, 8]) {
          produce T_power {
            for (ax1, 0, 384) {
              for (ax2, 0, 16) {
                for (ax3, 0, 8) {
                  T_power(0, ax1, ax2, ax3) =pow(T_subtract(0, ax1, ax2, ax3), 2.000000f)
                }
              }
            }
          }
          // attr [compute(T_sum, 0x3121a760)] realize_scope = ""
          realize T_sum([0, 1], [0, 384], [0, 1], [0, 1]) {
            produce T_sum {
              for (ax1, 0, 384) {
                T_sum(0, ax1, 0, 0) =0.000000f
                for (k2, 0, 16) {
                  for (k3, 0, 8) {
                    T_sum(0, ax1, 0, 0) =(T_sum(0, ax1, 0, 0) + T_power(0, ax1, k2, k3))
                  }
                }
              }
            }
            // attr [compute(T_divide, 0x1ad20870)] realize_scope = ""
            realize T_divide([0, 1], [0, 384], [0, 1], [0, 1]) {
              produce T_divide {
                for (ax1, 0, 384) {
                  T_divide(0, ax1, 0, 0) =(T_sum(0, ax1, 0, 0)/128.000000f)
                }
              }
              // attr [compute(T_add, 0x1c02d260)] realize_scope = ""
              realize T_add([0, 1], [0, 384], [0, 1], [0, 1]) {
                produce T_add {
                  for (ax1, 0, 384) {
                    T_add(0, ax1, 0, 0) =(T_divide(0, ax1, 0, 0) + 0.000010f)
                  }
                }
                // attr [compute(T_sqrt, 0x2d7c6280)] realize_scope = ""
                realize T_sqrt([0, 1], [0, 384], [0, 1], [0, 1]) {
                  produce T_sqrt {
                    for (ax1, 0, 384) {
                      T_sqrt(0, ax1, 0, 0) =sqrt(T_add(0, ax1, 0, 0))
                    }
                  }
                  // attr [compute(T_divide, 0x2db5a1e0)] realize_scope = ""
                  realize T_divide([0, 1], [0, 384], [0, 16], [0, 8]) {
                    produce T_divide {
                      for (ax1, 0, 384) {
                        for (ax2, 0, 16) {
                          for (ax3, 0, 8) {
                            T_divide(0, ax1, ax2, ax3) =(T_subtract(0, ax1, ax2, ax3)/T_sqrt(0, ax1, 0, 0))
                          }
                        }
                      }
                    }
                    // attr [compute(T_cast, 0x156989b0)] realize_scope = ""
                    realize T_cast([0, 1], [0, 384], [0, 16], [0, 8]) {
                      produce T_cast {
                        for (ax1, 0, 384) {
                          for (ax2, 0, 16) {
                            for (ax3, 0, 8) {
                              T_cast(0, ax1, ax2, ax3) =float16(T_divide(0, ax1, ax2, ax3))
                            }
                          }
                        }
                      }
                      // attr [compute(T_expand_dims, 0x30e6a590)] realize_scope = ""
                      realize T_expand_dims([0, 384], [0, 1], [0, 1]) {
                        produce T_expand_dims {
                          for (ax0, 0, 384) {
                            T_expand_dims(ax0, 0, 0) =placeholder(ax0)
                          }
                        }
                        // attr [compute(T_multiply, 0x1c117c40)] realize_scope = ""
                        realize T_multiply([0, 1], [0, 384], [0, 16], [0, 8]) {
                          produce T_multiply {
                            for (ax1, 0, 384) {
                              for (ax2, 0, 16) {
                                for (ax3, 0, 8) {
                                  T_multiply(0, ax1, ax2, ax3) =(T_cast(0, ax1, ax2, ax3)*T_expand_dims(ax1, 0, 0))
                                }
                              }
                            }
                          }
                          // attr [compute(T_expand_dims, 0x29c86510)] realize_scope = ""
                          realize T_expand_dims([0, 384], [0, 1], [0, 1]) {
                            produce T_expand_dims {
                              for (ax0, 0, 384) {
                                T_expand_dims(ax0, 0, 0) =placeholder(ax0)
                              }
                            }
                            // attr [compute(T_add, 0x25769cf0)] realize_scope = ""
                            realize T_add([0, 1], [0, 384], [0, 16], [0, 8]) {
                              produce T_add {
                                for (ax1, 0, 384) {
                                  for (ax2, 0, 16) {
                                    for (ax3, 0, 8) {
                                      T_add(0, ax1, ax2, ax3) =(T_multiply(0, ax1, ax2, ax3) + T_expand_dims(ax1, 0, 0))
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}
