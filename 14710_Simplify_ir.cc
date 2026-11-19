// attr [T_cast] storage_scope = "global"
allocate T_cast[float32 * 131072]
// attr [T_divide] storage_scope = "global"
allocate T_divide[float32 * 131072]
// attr [T_sum] storage_scope = "global"
allocate T_sum[float32 * 256]
produce T_cast {
  for (ax1, 0, 256) {
    for (ax2, 0, 32) {
      for (ax3, 0, 16) {
        T_cast[((((ax1*32) + ax2)*16) + ax3)] = float32(placeholder[((((ax1*32) + ax2)*16) + ax3)])
      }
    }
  }
}
produce T_divide {
  for (ax1, 0, 256) {
    for (ax2, 0, 32) {
      for (ax3, 0, 16) {
        T_divide[((((ax1*32) + ax2)*16) + ax3)] = (T_cast[((((ax1*32) + ax2)*16) + ax3)]*0.001953f)
      }
    }
  }
}
produce T_sum {
  for (ax1, 0, 256) {
    T_sum[ax1] = 0.000000f
    for (k2, 0, 32) {
      for (k3, 0, 16) {
        T_sum[ax1] = (T_sum[ax1] + T_divide[((((ax1*32) + k2)*16) + k3)])
      }
    }
  }
}
produce T_subtract {
  for (ax1, 0, 256) {
    for (ax2, 0, 32) {
      for (ax3, 0, 16) {
        T_cast[((((ax1*32) + ax2)*16) + ax3)] = (T_cast[((((ax1*32) + ax2)*16) + ax3)] - T_sum[ax1])
      }
    }
  }
}
produce T_power {
  for (ax1, 0, 256) {
    for (ax2, 0, 32) {
      for (ax3, 0, 16) {
        T_divide[((((ax1*32) + ax2)*16) + ax3)] = pow(T_cast[((((ax1*32) + ax2)*16) + ax3)], 2.000000f)
      }
    }
  }
}
produce T_sum {
  for (ax1, 0, 256) {
    T_sum[ax1] = 0.000000f
    for (k2, 0, 32) {
      for (k3, 0, 16) {
        T_sum[ax1] = (T_sum[ax1] + T_divide[((((ax1*32) + k2)*16) + k3)])
      }
    }
  }
}
produce T_divide {
  for (ax1, 0, 256) {
    T_sum[ax1] = (T_sum[ax1]*0.001953f)
  }
}
produce T_add {
  for (ax1, 0, 256) {
    T_sum[ax1] = (T_sum[ax1] + 0.000010f)
  }
}
produce T_sqrt {
  for (ax1, 0, 256) {
    T_sum[ax1] = sqrt(T_sum[ax1])
  }
}
produce T_divide {
  for (ax1, 0, 256) {
    for (ax2, 0, 32) {
      for (ax3, 0, 16) {
        T_cast[((((ax1*32) + ax2)*16) + ax3)] = (T_cast[((((ax1*32) + ax2)*16) + ax3)]/T_sum[ax1])
      }
    }
  }
}
produce T_cast {
  for (ax1, 0, 256) {
    for (ax2, 0, 32) {
      for (ax3, 0, 16) {
        T_divide[((((ax1*32) + ax2)*16) + ax3)] = float16(T_cast[((((ax1*32) + ax2)*16) + ax3)])
      }
    }
  }
}
produce T_expand_dims {
  for (ax0, 0, 256) {
    T_sum[ax0] = placeholder[ax0]
  }
}
produce T_multiply {
  for (ax1, 0, 256) {
    for (ax2, 0, 32) {
      for (ax3, 0, 16) {
        T_cast[((((ax1*32) + ax2)*16) + ax3)] = (T_divide[((((ax1*32) + ax2)*16) + ax3)]*T_sum[ax1])
      }
    }
  }
}
produce T_expand_dims {
  for (ax0, 0, 256) {
    T_sum[ax0] = placeholder[ax0]
  }
}
produce T_add {
  for (ax1, 0, 256) {
    for (ax2, 0, 32) {
      for (ax3, 0, 16) {
        T_add[((((ax1*32) + ax2)*16) + ax3)] = (T_cast[((((ax1*32) + ax2)*16) + ax3)] + T_sum[ax1])
      }
    }
  }
}
