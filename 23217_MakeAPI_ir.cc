assert((num_args == 3), "fused_nn_conv2d_35: num_args should be 3")
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
  assert(((((1 == int32(arg0.strides[3])) && (60 == int32(arg0.strides[2]))) && (1920 == int32(arg0.strides[1]))) && (97920 == int32(arg0.strides[0]))), "arg0.strides: expected to be compact array")
  0
}
let dev_type = tvm_struct_get(arg0, 0, 10)
let dev_id = tvm_struct_get(arg0, 0, 9)
let placeholder = tvm_struct_get(arg1, 0, 1)
// attr [placeholder] storage_alignment = 64
let arg1.shape = tvm_struct_get(arg1, 0, 2)
let arg1.strides = tvm_struct_get(arg1, 0, 3)
if (!tvm_handle_is_null(arg1.strides)) {
  assert(((((1 == int32(arg1.strides[3])) && (1 == int32(arg1.strides[2]))) && (1 == int32(arg1.strides[1]))) && (51 == int32(arg1.strides[0]))), "arg1.strides: expected to be compact array")
  0
}
let output_unpack = tvm_struct_get(arg2, 0, 1)
// attr [output_unpack] storage_alignment = 64
let arg2.shape = tvm_struct_get(arg2, 0, 2)
let arg2.strides = tvm_struct_get(arg2, 0, 3)
if (!tvm_handle_is_null(arg2.strides)) {
  assert(((((1 == int32(arg2.strides[3])) && (60 == int32(arg2.strides[2]))) && (1920 == int32(arg2.strides[1]))) && (97920 == int32(arg2.strides[0]))), "arg2.strides: expected to be compact array")
  0
}
assert(((((arg0.code == 3) || (arg0.code == 13)) || (arg0.code == 7)) || (arg0.code == 4)), "fused_nn_conv2d_35: Expect arg[0] to be pointer")
assert(((((arg1.code == 3) || (arg1.code == 13)) || (arg1.code == 7)) || (arg1.code == 4)), "fused_nn_conv2d_35: Expect arg[1] to be pointer")
assert(((((arg2.code == 3) || (arg2.code == 13)) || (arg2.code == 7)) || (arg2.code == 4)), "fused_nn_conv2d_35: Expect arg[2] to be pointer")
// attr ["default"] device_context_id = dev_id
// attr ["default"] device_context_type = dev_type
assert((4 == tvm_struct_get(arg0, 0, 4)), "arg0.ndim is expected to equal 4")
assert((((tvm_struct_get(arg0, 0, 5) == (uint8)0) && (tvm_struct_get(arg0, 0, 6) == (uint8)8)) && (tvm_struct_get(arg0, 0, 7) == (uint16)1)), "arg0.dtype is expected to be int8")
assert((((tvm_struct_get(arg0, 0, 5) == (uint8)0) && (tvm_struct_get(arg0, 0, 6) == (uint8)8)) && (tvm_struct_get(arg0, 0, 7) == (uint16)1)), "arg0.dtype is expected to be int8")
assert((4 == tvm_struct_get(arg0, 0, 4)), "arg0.ndim is expected to equal 4")
assert((int32(arg0.shape[0]) == 1), "Argument arg0.shape[0] has an unsatisfied constraint")
assert((int32(arg0.shape[1]) == 51), "Argument arg0.shape[1] has an unsatisfied constraint")
assert((int32(arg0.shape[2]) == 32), "Argument arg0.shape[2] has an unsatisfied constraint")
assert((int32(arg0.shape[3]) == 60), "Argument arg0.shape[3] has an unsatisfied constraint")
assert((tvm_struct_get(arg0, 0, 8) == (uint64)0), "Argument arg0.byte_offset has an unsatisfied constraint")
assert((4 == tvm_struct_get(arg1, 0, 4)), "arg1.ndim is expected to equal 4")
assert((((tvm_struct_get(arg1, 0, 5) == (uint8)0) && (tvm_struct_get(arg1, 0, 6) == (uint8)8)) && (tvm_struct_get(arg1, 0, 7) == (uint16)1)), "arg1.dtype is expected to be int8")
assert((((tvm_struct_get(arg1, 0, 5) == (uint8)0) && (tvm_struct_get(arg1, 0, 6) == (uint8)8)) && (tvm_struct_get(arg1, 0, 7) == (uint16)1)), "arg1.dtype is expected to be int8")
assert((4 == tvm_struct_get(arg1, 0, 4)), "arg1.ndim is expected to equal 4")
assert((int32(arg1.shape[0]) == 51), "Argument arg1.shape[0] has an unsatisfied constraint")
assert((int32(arg1.shape[1]) == 51), "Argument arg1.shape[1] has an unsatisfied constraint")
assert((int32(arg1.shape[2]) == 1), "Argument arg1.shape[2] has an unsatisfied constraint")
assert((int32(arg1.shape[3]) == 1), "Argument arg1.shape[3] has an unsatisfied constraint")
assert((tvm_struct_get(arg1, 0, 8) == (uint64)0), "Argument arg1.byte_offset has an unsatisfied constraint")
assert((dev_type == tvm_struct_get(arg1, 0, 10)), "Argument arg1.device_type has an unsatisfied constraint")
assert((dev_id == tvm_struct_get(arg1, 0, 9)), "Argument arg1.device_id has an unsatisfied constraint")
assert((4 == tvm_struct_get(arg2, 0, 4)), "arg2.ndim is expected to equal 4")
assert((((tvm_struct_get(arg2, 0, 5) == (uint8)0) && (tvm_struct_get(arg2, 0, 6) == (uint8)64)) && (tvm_struct_get(arg2, 0, 7) == (uint16)1)), "arg2.dtype is expected to be int64")
assert((((tvm_struct_get(arg2, 0, 5) == (uint8)0) && (tvm_struct_get(arg2, 0, 6) == (uint8)64)) && (tvm_struct_get(arg2, 0, 7) == (uint16)1)), "arg2.dtype is expected to be int64")
assert((4 == tvm_struct_get(arg2, 0, 4)), "arg2.ndim is expected to equal 4")
assert((int32(arg2.shape[0]) == 1), "Argument arg2.shape[0] has an unsatisfied constraint")
assert((int32(arg2.shape[1]) == 51), "Argument arg2.shape[1] has an unsatisfied constraint")
assert((int32(arg2.shape[2]) == 32), "Argument arg2.shape[2] has an unsatisfied constraint")
assert((int32(arg2.shape[3]) == 60), "Argument arg2.shape[3] has an unsatisfied constraint")
assert((tvm_struct_get(arg2, 0, 8) == (uint64)0), "Argument arg2.byte_offset has an unsatisfied constraint")
assert((dev_type == tvm_struct_get(arg2, 0, 10)), "Argument arg2.device_type has an unsatisfied constraint")
assert((dev_id == tvm_struct_get(arg2, 0, 9)), "Argument arg2.device_id has an unsatisfied constraint")
if ((dev_type != 1)) {
  tvm_call_packed("__tvm_set_device", dev_type, dev_id)
}
// attr [0] compute_scope = "fused_nn_conv2d_35_compute_"
// attr [data_vec] storage_scope = "global"
allocate data_vec[int8 * 97920]
// attr [kernel_vec] storage_scope = "global"
allocate kernel_vec[int8x3 * 867]
produce data_vec {
  parallel (C.h.fused, 0, 544) {
    for (c, 0, 3) {
      for (w, 0, 60) {
        data_vec[((((C.h.fused*3) + c)*60) + w)] = placeholder[(((((((C.h.fused/32)*3) + c)*32) + (C.h.fused % 32))*60) + w)]
      }
    }
  }
}
produce kernel_vec {
  parallel (CO.h.fused, 0, 17) {
    for (CI, 0, 17) {
      for (ci, 0, 3) {
        kernel_vec[ramp((((((CO.h.fused*17) + CI)*3) + ci)*3), 1, 3)] = placeholder[ramp(((((CO.h.fused*51) + CI)*3) + ci), 51, 3)]
      }
    }
  }
}
produce output_unpack {
  parallel (c.outer.h.outer.fused, 0, 544) {
    // attr [conv.global] storage_scope = "global"
    allocate conv.global[int64x3 * 60]
    produce conv {
      produce conv.global {
        for (ow.c.outer, 0, 2) {
          conv.global[ramp((ow.c.outer*90), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 3), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 6), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 9), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 12), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 15), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 18), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 21), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 24), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 27), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 30), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 33), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 36), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 39), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 42), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 45), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 48), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 51), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 54), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 57), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 60), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 63), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 66), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 69), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 72), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 75), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 78), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 81), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 84), 1, 3)] = x3((int64)0)
          conv.global[ramp(((ow.c.outer*90) + 87), 1, 3)] = x3((int64)0)
          for (ic.outer, 0, 17) {
            for (ic.inner, 0, 3) {
              conv.global[ramp((ow.c.outer*90), 1, 3)] = (conv.global[ramp((ow.c.outer*90), 1, 3)] + (x3(int64(data_vec[(((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 3), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 3), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 1)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 6), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 6), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 2)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 9), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 9), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 3)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 12), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 12), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 4)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 15), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 15), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 5)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 18), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 18), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 6)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 21), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 21), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 7)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 24), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 24), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 8)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 27), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 27), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 9)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 30), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 30), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 10)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 33), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 33), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 11)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 36), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 36), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 12)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 39), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 39), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 13)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 42), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 42), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 14)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 45), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 45), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 15)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 48), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 48), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 16)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 51), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 51), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 17)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 54), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 54), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 18)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 57), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 57), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 19)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 60), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 60), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 20)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 63), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 63), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 21)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 66), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 66), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 22)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 69), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 69), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 23)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 72), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 72), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 24)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 75), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 75), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 25)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 78), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 78), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 26)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 81), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 81), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 27)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 84), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 84), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 28)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
              conv.global[ramp(((ow.c.outer*90) + 87), 1, 3)] = (conv.global[ramp(((ow.c.outer*90) + 87), 1, 3)] + (x3(int64(data_vec[((((((((ic.outer*32) + (c.outer.h.outer.fused % 32))*3) + ic.inner)*2) + ow.c.outer)*30) + 29)]))*int64x3(kernel_vec[ramp(((((((c.outer.h.outer.fused/32)*17) + ic.outer)*3) + ic.inner)*3), 1, 3)])))
            }
          }
        }
      }
    }
    for (w.outer, 0, 2) {
      for (w.inner, 0, 30) {
        output_unpack[ramp((((((((c.outer.h.outer.fused/32)*96) + (c.outer.h.outer.fused % 32))*2) + w.outer)*30) + w.inner), 1920, 3)] = conv.global[ramp((((w.outer*30) + w.inner)*3), 1, 3)]
      }
    }
  }
}
