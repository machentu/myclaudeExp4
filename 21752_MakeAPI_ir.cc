assert((num_args == 3), "fused_nn_conv2d_27: num_args should be 3")
let arg0 = tvm_struct_get(args, 0, 12)
let arg0.code = arg_type_ids[0]
let arg1 = tvm_struct_get(args, 1, 12)
let arg1.code = arg_type_ids[1]
let arg2 = tvm_struct_get(args, 2, 12)
let arg2.code = arg_type_ids[2]
let placeholder = tvm_struct_get(arg0, 0, 1)
// attr [placeholder] storage_alignment = 64
let arg0.shape = tvm_struct_get(arg0, 0, 2)
let arg0.strides = tvm_struct_get(arg0, 0, 3)
if (!tvm_handle_is_null(arg0.strides)) {
  assert(((((1 == int32(arg0.strides[3])) && (60 == int32(arg0.strides[2]))) && (1920 == int32(arg0.strides[1]))) && (122880 == int32(arg0.strides[0]))), "arg0.strides: expected to be compact array")
  0
}
let dev_type = tvm_struct_get(arg0, 0, 10)
let dev_id = tvm_struct_get(arg0, 0, 9)
let placeholder = tvm_struct_get(arg1, 0, 1)
// attr [placeholder] storage_alignment = 64
let arg1.shape = tvm_struct_get(arg1, 0, 2)
let arg1.strides = tvm_struct_get(arg1, 0, 3)
if (!tvm_handle_is_null(arg1.strides)) {
  assert(((((1 == int32(arg1.strides[3])) && (3 == int32(arg1.strides[2]))) && (9 == int32(arg1.strides[1]))) && (576 == int32(arg1.strides[0]))), "arg1.strides: expected to be compact array")
  0
}
let output_unpack = tvm_struct_get(arg2, 0, 1)
// attr [output_unpack] storage_alignment = 64
let arg2.shape = tvm_struct_get(arg2, 0, 2)
let arg2.strides = tvm_struct_get(arg2, 0, 3)
if (!tvm_handle_is_null(arg2.strides)) {
  assert(((((1 == int32(arg2.strides[3])) && (60 == int32(arg2.strides[2]))) && (1920 == int32(arg2.strides[1]))) && (153600 == int32(arg2.strides[0]))), "arg2.strides: expected to be compact array")
  0
}
assert(((((arg0.code == 3) || (arg0.code == 13)) || (arg0.code == 7)) || (arg0.code == 4)), "fused_nn_conv2d_27: Expect arg[0] to be pointer")
assert(((((arg1.code == 3) || (arg1.code == 13)) || (arg1.code == 7)) || (arg1.code == 4)), "fused_nn_conv2d_27: Expect arg[1] to be pointer")
assert(((((arg2.code == 3) || (arg2.code == 13)) || (arg2.code == 7)) || (arg2.code == 4)), "fused_nn_conv2d_27: Expect arg[2] to be pointer")
// attr ["default"] device_context_id = dev_id
// attr ["default"] device_context_type = dev_type
assert((4 == tvm_struct_get(arg0, 0, 4)), "arg0.ndim is expected to equal 4")
assert((((tvm_struct_get(arg0, 0, 5) == (uint8)0) && (tvm_struct_get(arg0, 0, 6) == (uint8)8)) && (tvm_struct_get(arg0, 0, 7) == (uint16)1)), "arg0.dtype is expected to be int8")
assert((((tvm_struct_get(arg0, 0, 5) == (uint8)0) && (tvm_struct_get(arg0, 0, 6) == (uint8)8)) && (tvm_struct_get(arg0, 0, 7) == (uint16)1)), "arg0.dtype is expected to be int8")
assert((4 == tvm_struct_get(arg0, 0, 4)), "arg0.ndim is expected to equal 4")
assert((int32(arg0.shape[0]) == 1), "Argument arg0.shape[0] has an unsatisfied constraint")
assert((int32(arg0.shape[1]) == 64), "Argument arg0.shape[1] has an unsatisfied constraint")
assert((int32(arg0.shape[2]) == 32), "Argument arg0.shape[2] has an unsatisfied constraint")
assert((int32(arg0.shape[3]) == 60), "Argument arg0.shape[3] has an unsatisfied constraint")
assert((tvm_struct_get(arg0, 0, 8) == (uint64)0), "Argument arg0.byte_offset has an unsatisfied constraint")
assert((4 == tvm_struct_get(arg1, 0, 4)), "arg1.ndim is expected to equal 4")
assert((((tvm_struct_get(arg1, 0, 5) == (uint8)0) && (tvm_struct_get(arg1, 0, 6) == (uint8)8)) && (tvm_struct_get(arg1, 0, 7) == (uint16)1)), "arg1.dtype is expected to be int8")
assert((((tvm_struct_get(arg1, 0, 5) == (uint8)0) && (tvm_struct_get(arg1, 0, 6) == (uint8)8)) && (tvm_struct_get(arg1, 0, 7) == (uint16)1)), "arg1.dtype is expected to be int8")
assert((4 == tvm_struct_get(arg1, 0, 4)), "arg1.ndim is expected to equal 4")
assert((int32(arg1.shape[0]) == 80), "Argument arg1.shape[0] has an unsatisfied constraint")
assert((int32(arg1.shape[1]) == 64), "Argument arg1.shape[1] has an unsatisfied constraint")
assert((int32(arg1.shape[2]) == 3), "Argument arg1.shape[2] has an unsatisfied constraint")
assert((int32(arg1.shape[3]) == 3), "Argument arg1.shape[3] has an unsatisfied constraint")
assert((tvm_struct_get(arg1, 0, 8) == (uint64)0), "Argument arg1.byte_offset has an unsatisfied constraint")
assert((dev_type == tvm_struct_get(arg1, 0, 10)), "Argument arg1.device_type has an unsatisfied constraint")
assert((dev_id == tvm_struct_get(arg1, 0, 9)), "Argument arg1.device_id has an unsatisfied constraint")
assert((4 == tvm_struct_get(arg2, 0, 4)), "arg2.ndim is expected to equal 4")
assert((((tvm_struct_get(arg2, 0, 5) == (uint8)0) && (tvm_struct_get(arg2, 0, 6) == (uint8)64)) && (tvm_struct_get(arg2, 0, 7) == (uint16)1)), "arg2.dtype is expected to be int64")
assert((((tvm_struct_get(arg2, 0, 5) == (uint8)0) && (tvm_struct_get(arg2, 0, 6) == (uint8)64)) && (tvm_struct_get(arg2, 0, 7) == (uint16)1)), "arg2.dtype is expected to be int64")
assert((4 == tvm_struct_get(arg2, 0, 4)), "arg2.ndim is expected to equal 4")
assert((int32(arg2.shape[0]) == 1), "Argument arg2.shape[0] has an unsatisfied constraint")
assert((int32(arg2.shape[1]) == 80), "Argument arg2.shape[1] has an unsatisfied constraint")
assert((int32(arg2.shape[2]) == 32), "Argument arg2.shape[2] has an unsatisfied constraint")
assert((int32(arg2.shape[3]) == 60), "Argument arg2.shape[3] has an unsatisfied constraint")
assert((tvm_struct_get(arg2, 0, 8) == (uint64)0), "Argument arg2.byte_offset has an unsatisfied constraint")
assert((dev_type == tvm_struct_get(arg2, 0, 10)), "Argument arg2.device_type has an unsatisfied constraint")
assert((dev_id == tvm_struct_get(arg2, 0, 9)), "Argument arg2.device_id has an unsatisfied constraint")
if ((dev_type != 1)) {
  tvm_call_packed("__tvm_set_device", dev_type, dev_id)
}
// attr [0] compute_scope = "fused_nn_conv2d_27_compute_"
// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 134912]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x8 * 5760]
produce data_vec {
  parallel (C.h.fused, 0, 272) {
    for (c, 0, 8) {
      for (w, 0, 62) {
        data_vec[((((C.h.fused*8) + c)*62) + w)] = tvm_if_then_else(((((1 <= (C.h.fused % 34)) && ((C.h.fused % 34) < 33)) && (1 <= w)) && (w < 61)), placeholder[((((((((C.h.fused/34)*8) + c)*32) + (C.h.fused % 34))*60) + w) + -61)], (int8)0)
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 30) {
    for (CI, 0, 8) {
      for (w, 0, 3) {
        for (ci, 0, 8) {
          kernel_vec[ramp(((((((((((CO.h.fused/3)*8) + CI)*3) + (CO.h.fused % 3))*3) + w)*8) + ci)*8), 1, 8)] = placeholder[ramp((((((((((CO.h.fused/3)*64) + CI)*8) + ci)*3) + (CO.h.fused % 3))*3) + w), 576, 8)]
        }
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.fused, 0, 320) {
    // attr [conv] storage_scope = "global"
    allocate conv[int64x8 * 60]
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x8 * 30]
    produce conv {
      for (ow.outer, 0, 2) {
        produce conv.global {
          conv.global[ramp(0, 1, 8)] = x8((int64)0)
          conv.global[ramp(8, 1, 8)] = x8((int64)0)
          conv.global[ramp(16, 1, 8)] = x8((int64)0)
          conv.global[ramp(24, 1, 8)] = x8((int64)0)
          conv.global[ramp(32, 1, 8)] = x8((int64)0)
          conv.global[ramp(40, 1, 8)] = x8((int64)0)
          conv.global[ramp(48, 1, 8)] = x8((int64)0)
          conv.global[ramp(56, 1, 8)] = x8((int64)0)
          conv.global[ramp(64, 1, 8)] = x8((int64)0)
          conv.global[ramp(72, 1, 8)] = x8((int64)0)
          conv.global[ramp(80, 1, 8)] = x8((int64)0)
          conv.global[ramp(88, 1, 8)] = x8((int64)0)
          conv.global[ramp(96, 1, 8)] = x8((int64)0)
          conv.global[ramp(104, 1, 8)] = x8((int64)0)
          conv.global[ramp(112, 1, 8)] = x8((int64)0)
          conv.global[ramp(120, 1, 8)] = x8((int64)0)
          conv.global[ramp(128, 1, 8)] = x8((int64)0)
          conv.global[ramp(136, 1, 8)] = x8((int64)0)
          conv.global[ramp(144, 1, 8)] = x8((int64)0)
          conv.global[ramp(152, 1, 8)] = x8((int64)0)
          conv.global[ramp(160, 1, 8)] = x8((int64)0)
          conv.global[ramp(168, 1, 8)] = x8((int64)0)
          conv.global[ramp(176, 1, 8)] = x8((int64)0)
          conv.global[ramp(184, 1, 8)] = x8((int64)0)
          conv.global[ramp(192, 1, 8)] = x8((int64)0)
          conv.global[ramp(200, 1, 8)] = x8((int64)0)
          conv.global[ramp(208, 1, 8)] = x8((int64)0)
          conv.global[ramp(216, 1, 8)] = x8((int64)0)
          conv.global[ramp(224, 1, 8)] = x8((int64)0)
          conv.global[ramp(232, 1, 8)] = x8((int64)0)
          for (ic.outer, 0, 8) {
            for (kh, 0, 3) {
              for (kw, 0, 3) {
                for (ic.inner, 0, 8) {
                  conv.global[ramp(0, 1, 8)] = (conv.global[ramp(0, 1, 8)] + (x8(int64(data_vec[((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(8, 1, 8)] = (conv.global[ramp(8, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 1)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(16, 1, 8)] = (conv.global[ramp(16, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 2)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(24, 1, 8)] = (conv.global[ramp(24, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 3)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(32, 1, 8)] = (conv.global[ramp(32, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 4)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(40, 1, 8)] = (conv.global[ramp(40, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 5)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(48, 1, 8)] = (conv.global[ramp(48, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 6)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(56, 1, 8)] = (conv.global[ramp(56, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 7)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(64, 1, 8)] = (conv.global[ramp(64, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 8)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(72, 1, 8)] = (conv.global[ramp(72, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 9)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(80, 1, 8)] = (conv.global[ramp(80, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 10)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(88, 1, 8)] = (conv.global[ramp(88, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 11)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(96, 1, 8)] = (conv.global[ramp(96, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 12)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(104, 1, 8)] = (conv.global[ramp(104, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 13)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(112, 1, 8)] = (conv.global[ramp(112, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 14)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(120, 1, 8)] = (conv.global[ramp(120, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 15)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(128, 1, 8)] = (conv.global[ramp(128, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 16)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(136, 1, 8)] = (conv.global[ramp(136, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 17)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(144, 1, 8)] = (conv.global[ramp(144, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 18)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(152, 1, 8)] = (conv.global[ramp(152, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 19)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(160, 1, 8)] = (conv.global[ramp(160, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 20)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(168, 1, 8)] = (conv.global[ramp(168, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 21)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(176, 1, 8)] = (conv.global[ramp(176, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 22)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(184, 1, 8)] = (conv.global[ramp(184, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 23)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(192, 1, 8)] = (conv.global[ramp(192, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 24)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(200, 1, 8)] = (conv.global[ramp(200, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 25)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(208, 1, 8)] = (conv.global[ramp(208, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 26)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(216, 1, 8)] = (conv.global[ramp(216, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 27)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(224, 1, 8)] = (conv.global[ramp(224, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 28)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                  conv.global[ramp(232, 1, 8)] = (conv.global[ramp(232, 1, 8)] + (x8(int64(data_vec[(((((((((ic.outer*34) + kh) + (c.outer.h.fused % 32))*8) + ic.inner)*62) + (ow.outer*30)) + kw) + 29)]))*int64x8(kernel_vec[ramp(((((((((((c.outer.h.fused/32)*8) + ic.outer)*3) + kh)*3) + kw)*8) + ic.inner)*8), 1, 8)])))
                }
              }
            }
          }
        }
        for (ow.inner, 0, 30) {
          conv[ramp((((ow.outer*30) + ow.inner)*8), 1, 8)] = conv.global[ramp((ow.inner*8), 1, 8)]
        }
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 30) {
        output_unpack[ramp((((((((c.outer.h.fused/32)*256) + (c.outer.h.fused % 32))*2) + w.outer)*30) + w.inner), 1920, 8)] = conv[ramp((((w.outer*30) + w.inner)*8), 1, 8)]
      }
    }
  }
}
