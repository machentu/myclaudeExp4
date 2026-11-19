// attr [T_cast] storage_scope = "global"
allocate T_cast[float32 * 1 * 256 * 32 * 16]
produce T_cast {
  for (ax1, 0, 256) {
    for (ax2, 0, 32) {
      for (ax3, 0, 16) {
        T_cast[((((ax1*32) + ax2)*16) + ax3)] = float32(placeholder[((((ax1*32) + ax2)*16) + ax3)])
      }
    }
  }
}
// attr [T_divide] storage_scope = "global"
allocate T_divide[float32 * 1 * 256 * 32 * 16]
produce T_divide {
  for (ax1, 0, 256) {
    for (ax2, 0, 32) {
      for (ax3, 0, 16) {
        T_divide[((((ax1*32) + ax2)*16) + ax3)] = (T_cast[((((ax1*32) + ax2)*16) + ax3)]/512.000000f)
      }
    }
  }
}
// attr [T_sum] storage_scope = "global"
allocate T_sum[float32 * 1 * 256 * 1 * 1]
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
// attr [T_subtract] storage_scope = "global"
allocate T_subtract[float32 * 1 * 256 * 32 * 16]
produce T_subtract {
  for (ax1, 0, 256) {
    for (ax2, 0, 32) {
      for (ax3, 0, 16) {
        T_subtract[((((ax1*32) + ax2)*16) + ax3)] = (T_cast[((((ax1*32) + ax2)*16) + ax3)] - T_sum[ax1])
      }
    }
  }
}
// attr [T_power] storage_scope = "global"
allocate T_power[float32 * 1 * 256 * 32 * 16]
produce T_power {
  for (ax1, 0, 256) {
    for (ax2, 0, 32) {
      for (ax3, 0, 16) {
        T_power[((((ax1*32) + ax2)*16) + ax3)] = pow(T_subtract[((((ax1*32) + ax2)*16) + ax3)], 2.000000f)
      }
    }
  }
}
// attr [T_sum] storage_scope = "global"
allocate T_sum[float32 * 1 * 256 * 1 * 1]
produce T_sum {
  for (ax1, 0, 256) {
    T_sum[ax1] = 0.000000f
    for (k2, 0, 32) {
      for (k3, 0, 16) {
        T_sum[ax1] = (T_sum[ax1] + T_power[((((ax1*32) + k2)*16) + k3)])
      }
    }
  }
}
// attr [T_divide] storage_scope = "global"
allocate T_divide[float32 * 1 * 256 * 1 * 1]
produce T_divide {
  for (ax1, 0, 256) {
    T_divide[ax1] = (T_sum[ax1]/512.000000f)
  }
}
// attr [T_add] storage_scope = "global"
allocate T_add[float32 * 1 * 256 * 1 * 1]
produce T_add {
  for (ax1, 0, 256) {
    T_add[ax1] = (T_divide[ax1] + 0.000010f)
  }
}
// attr [T_sqrt] storage_scope = "global"
allocate T_sqrt[float32 * 1 * 256 * 1 * 1]
produce T_sqrt {
  for (ax1, 0, 256) {
    T_sqrt[ax1] = sqrt(T_add[ax1])
  }
}
// attr [T_divide] storage_scope = "global"
allocate T_divide[float32 * 1 * 256 * 32 * 16]
produce T_divide {
  for (ax1, 0, 256) {
    for (ax2, 0, 32) {
      for (ax3, 0, 16) {
        T_divide[((((ax1*32) + ax2)*16) + ax3)] = (T_subtract[((((ax1*32) + ax2)*16) + ax3)]/T_sqrt[ax1])
      }
    }
  }
}
// attr [T_cast] storage_scope = "global"
allocate T_cast[float16 * 1 * 256 * 32 * 16]
produce T_cast {
  for (ax1, 0, 256) {
    for (ax2, 0, 32) {
      for (ax3, 0, 16) {
        T_cast[((((ax1*32) + ax2)*16) + ax3)] = float16(T_divide[((((ax1*32) + ax2)*16) + ax3)])
      }
    }
  }
}
// attr [T_expand_dims] storage_scope = "global"
allocate T_expand_dims[float16 * 256 * 1 * 1]
produce T_expand_dims {
  for (ax0, 0, 256) {
    T_expand_dims[ax0] = placeholder[ax0]
  }
}
// attr [T_multiply] storage_scope = "global"
allocate T_multiply[float16 * 1 * 256 * 32 * 16]
produce T_multiply {
  for (ax1, 0, 256) {
    for (ax2, 0, 32) {
      for (ax3, 0, 16) {
        T_multiply[((((ax1*32) + ax2)*16) + ax3)] = (T_cast[((((ax1*32) + ax2)*16) + ax3)]*T_expand_dims[ax1])
      }
    }
  }
}
// attr [T_expand_dims] storage_scope = "global"
allocate T_expand_dims[float16 * 256 * 1 * 1]
produce T_expand_dims {
  for (ax0, 0, 256) {
    T_expand_dims[ax0] = placeholder[ax0]
  }
}
produce T_add {
  for (ax1, 0, 256) {
    for (ax2, 0, 32) {
      for (ax3, 0, 16) {
        T_add[((((ax1*32) + ax2)*16) + ax3)] = (T_multiply[((((ax1*32) + ax2)*16) + ax3)] + T_expand_dims[ax1])
      }
    }
  }
}
