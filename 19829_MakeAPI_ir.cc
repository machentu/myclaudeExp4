assert((num_args == 5), "fused_vision_yolov8_post_process: num_args should be 5")
let arg0 = tvm_struct_get(args, 0, 12)
let arg0.code = arg_type_ids[0]
let arg1 = tvm_struct_get(args, 1, 12)
let arg1.code = arg_type_ids[1]
let arg2 = tvm_struct_get(args, 2, 12)
let arg2.code = arg_type_ids[2]
let arg3 = tvm_struct_get(args, 3, 12)
let arg3.code = arg_type_ids[3]
let arg4 = tvm_struct_get(args, 4, 12)
let arg4.code = arg_type_ids[4]
let placeholder = tvm_struct_get(arg0, 0, 1)
// attr [placeholder] storage_alignment = 64
let arg0.shape = tvm_struct_get(arg0, 0, 2)
let arg0.strides = tvm_struct_get(arg0, 0, 3)
if (!tvm_handle_is_null(arg0.strides)) {
  assert((((1 == int32(arg0.strides[2])) && (2520 == int32(arg0.strides[1]))) && (10080 == int32(arg0.strides[0]))), "arg0.strides: expected to be compact array")
  0
}
let dev_type = tvm_struct_get(arg0, 0, 10)
let dev_id = tvm_struct_get(arg0, 0, 9)
let placeholder = tvm_struct_get(arg1, 0, 1)
// attr [placeholder] storage_alignment = 64
let arg1.shape = tvm_struct_get(arg1, 0, 2)
let arg1.strides = tvm_struct_get(arg1, 0, 3)
if (!tvm_handle_is_null(arg1.strides)) {
  assert((((1 == int32(arg1.strides[2])) && (2520 == int32(arg1.strides[1]))) && (5040 == int32(arg1.strides[0]))), "arg1.strides: expected to be compact array")
  0
}
let placeholder = tvm_struct_get(arg2, 0, 1)
// attr [placeholder] storage_alignment = 64
let arg2.shape = tvm_struct_get(arg2, 0, 2)
let arg2.strides = tvm_struct_get(arg2, 0, 3)
if (!tvm_handle_is_null(arg2.strides)) {
  assert((((1 == int32(arg2.strides[2])) && (2520 == int32(arg2.strides[1]))) && (5040 == int32(arg2.strides[0]))), "arg2.strides: expected to be compact array")
  0
}
let placeholder = tvm_struct_get(arg3, 0, 1)
// attr [placeholder] storage_alignment = 64
let arg3.shape = tvm_struct_get(arg3, 0, 2)
let arg3.strides = tvm_struct_get(arg3, 0, 3)
if (!tvm_handle_is_null(arg3.strides)) {
  assert(((1 == int32(arg3.strides[1])) && (2520 == int32(arg3.strides[0]))), "arg3.strides: expected to be compact array")
  0
}
let yolov8_post_process = tvm_struct_get(arg4, 0, 1)
// attr [yolov8_post_process] storage_alignment = 64
let arg4.shape = tvm_struct_get(arg4, 0, 2)
let arg4.strides = tvm_struct_get(arg4, 0, 3)
if (!tvm_handle_is_null(arg4.strides)) {
  assert((((1 == int32(arg4.strides[2])) && (2520 == int32(arg4.strides[1]))) && (10080 == int32(arg4.strides[0]))), "arg4.strides: expected to be compact array")
  0
}
assert(((((arg0.code == 3) || (arg0.code == 13)) || (arg0.code == 7)) || (arg0.code == 4)), "fused_vision_yolov8_post_process: Expect arg[0] to be pointer")
assert(((((arg1.code == 3) || (arg1.code == 13)) || (arg1.code == 7)) || (arg1.code == 4)), "fused_vision_yolov8_post_process: Expect arg[1] to be pointer")
assert(((((arg2.code == 3) || (arg2.code == 13)) || (arg2.code == 7)) || (arg2.code == 4)), "fused_vision_yolov8_post_process: Expect arg[2] to be pointer")
assert(((((arg3.code == 3) || (arg3.code == 13)) || (arg3.code == 7)) || (arg3.code == 4)), "fused_vision_yolov8_post_process: Expect arg[3] to be pointer")
assert(((((arg4.code == 3) || (arg4.code == 13)) || (arg4.code == 7)) || (arg4.code == 4)), "fused_vision_yolov8_post_process: Expect arg[4] to be pointer")
// attr ["default"] device_context_id = dev_id
// attr ["default"] device_context_type = dev_type
assert((3 == tvm_struct_get(arg0, 0, 4)), "arg0.ndim is expected to equal 3")
assert((((tvm_struct_get(arg0, 0, 5) == (uint8)2) && (tvm_struct_get(arg0, 0, 6) == (uint8)16)) && (tvm_struct_get(arg0, 0, 7) == (uint16)1)), "arg0.dtype is expected to be float16")
assert((((tvm_struct_get(arg0, 0, 5) == (uint8)2) && (tvm_struct_get(arg0, 0, 6) == (uint8)16)) && (tvm_struct_get(arg0, 0, 7) == (uint16)1)), "arg0.dtype is expected to be float16")
assert((3 == tvm_struct_get(arg0, 0, 4)), "arg0.ndim is expected to equal 3")
assert((int32(arg0.shape[0]) == 1), "Argument arg0.shape[0] has an unsatisfied constraint")
assert((int32(arg0.shape[1]) == 4), "Argument arg0.shape[1] has an unsatisfied constraint")
assert((int32(arg0.shape[2]) == 2520), "Argument arg0.shape[2] has an unsatisfied constraint")
assert((tvm_struct_get(arg0, 0, 8) == (uint64)0), "Argument arg0.byte_offset has an unsatisfied constraint")
assert((3 == tvm_struct_get(arg1, 0, 4)), "arg1.ndim is expected to equal 3")
assert((((tvm_struct_get(arg1, 0, 5) == (uint8)2) && (tvm_struct_get(arg1, 0, 6) == (uint8)16)) && (tvm_struct_get(arg1, 0, 7) == (uint16)1)), "arg1.dtype is expected to be float16")
assert((((tvm_struct_get(arg1, 0, 5) == (uint8)2) && (tvm_struct_get(arg1, 0, 6) == (uint8)16)) && (tvm_struct_get(arg1, 0, 7) == (uint16)1)), "arg1.dtype is expected to be float16")
assert((3 == tvm_struct_get(arg1, 0, 4)), "arg1.ndim is expected to equal 3")
assert((int32(arg1.shape[0]) == 1), "Argument arg1.shape[0] has an unsatisfied constraint")
assert((int32(arg1.shape[1]) == 2), "Argument arg1.shape[1] has an unsatisfied constraint")
assert((int32(arg1.shape[2]) == 2520), "Argument arg1.shape[2] has an unsatisfied constraint")
assert((tvm_struct_get(arg1, 0, 8) == (uint64)0), "Argument arg1.byte_offset has an unsatisfied constraint")
assert((dev_type == tvm_struct_get(arg1, 0, 10)), "Argument arg1.device_type has an unsatisfied constraint")
assert((dev_id == tvm_struct_get(arg1, 0, 9)), "Argument arg1.device_id has an unsatisfied constraint")
assert((3 == tvm_struct_get(arg2, 0, 4)), "arg2.ndim is expected to equal 3")
assert((((tvm_struct_get(arg2, 0, 5) == (uint8)2) && (tvm_struct_get(arg2, 0, 6) == (uint8)16)) && (tvm_struct_get(arg2, 0, 7) == (uint16)1)), "arg2.dtype is expected to be float16")
assert((((tvm_struct_get(arg2, 0, 5) == (uint8)2) && (tvm_struct_get(arg2, 0, 6) == (uint8)16)) && (tvm_struct_get(arg2, 0, 7) == (uint16)1)), "arg2.dtype is expected to be float16")
assert((3 == tvm_struct_get(arg2, 0, 4)), "arg2.ndim is expected to equal 3")
assert((int32(arg2.shape[0]) == 1), "Argument arg2.shape[0] has an unsatisfied constraint")
assert((int32(arg2.shape[1]) == 2), "Argument arg2.shape[1] has an unsatisfied constraint")
assert((int32(arg2.shape[2]) == 2520), "Argument arg2.shape[2] has an unsatisfied constraint")
assert((tvm_struct_get(arg2, 0, 8) == (uint64)0), "Argument arg2.byte_offset has an unsatisfied constraint")
assert((dev_type == tvm_struct_get(arg2, 0, 10)), "Argument arg2.device_type has an unsatisfied constraint")
assert((dev_id == tvm_struct_get(arg2, 0, 9)), "Argument arg2.device_id has an unsatisfied constraint")
assert((2 == tvm_struct_get(arg3, 0, 4)), "arg3.ndim is expected to equal 2")
assert((((tvm_struct_get(arg3, 0, 5) == (uint8)2) && (tvm_struct_get(arg3, 0, 6) == (uint8)16)) && (tvm_struct_get(arg3, 0, 7) == (uint16)1)), "arg3.dtype is expected to be float16")
assert((((tvm_struct_get(arg3, 0, 5) == (uint8)2) && (tvm_struct_get(arg3, 0, 6) == (uint8)16)) && (tvm_struct_get(arg3, 0, 7) == (uint16)1)), "arg3.dtype is expected to be float16")
assert((2 == tvm_struct_get(arg3, 0, 4)), "arg3.ndim is expected to equal 2")
assert((int32(arg3.shape[0]) == 1), "Argument arg3.shape[0] has an unsatisfied constraint")
assert((int32(arg3.shape[1]) == 2520), "Argument arg3.shape[1] has an unsatisfied constraint")
assert((tvm_struct_get(arg3, 0, 8) == (uint64)0), "Argument arg3.byte_offset has an unsatisfied constraint")
assert((dev_type == tvm_struct_get(arg3, 0, 10)), "Argument arg3.device_type has an unsatisfied constraint")
assert((dev_id == tvm_struct_get(arg3, 0, 9)), "Argument arg3.device_id has an unsatisfied constraint")
assert((3 == tvm_struct_get(arg4, 0, 4)), "arg4.ndim is expected to equal 3")
assert((((tvm_struct_get(arg4, 0, 5) == (uint8)2) && (tvm_struct_get(arg4, 0, 6) == (uint8)16)) && (tvm_struct_get(arg4, 0, 7) == (uint16)1)), "arg4.dtype is expected to be float16")
assert((((tvm_struct_get(arg4, 0, 5) == (uint8)2) && (tvm_struct_get(arg4, 0, 6) == (uint8)16)) && (tvm_struct_get(arg4, 0, 7) == (uint16)1)), "arg4.dtype is expected to be float16")
assert((3 == tvm_struct_get(arg4, 0, 4)), "arg4.ndim is expected to equal 3")
assert((int32(arg4.shape[0]) == 1), "Argument arg4.shape[0] has an unsatisfied constraint")
assert((int32(arg4.shape[1]) == 4), "Argument arg4.shape[1] has an unsatisfied constraint")
assert((int32(arg4.shape[2]) == 2520), "Argument arg4.shape[2] has an unsatisfied constraint")
assert((tvm_struct_get(arg4, 0, 8) == (uint64)0), "Argument arg4.byte_offset has an unsatisfied constraint")
assert((dev_type == tvm_struct_get(arg4, 0, 10)), "Argument arg4.device_type has an unsatisfied constraint")
assert((dev_id == tvm_struct_get(arg4, 0, 9)), "Argument arg4.device_id has an unsatisfied constraint")
if ((dev_type != 1)) {
  tvm_call_packed("__tvm_set_device", dev_type, dev_id)
}
// attr [0] compute_scope = "fused_vision_yolov8_post_process_compute_"
produce yolov8_post_process {
  // attr [0] extern_scope = 0
  tvm_call_packed("tvm_contrib_yolov8_post_process", tvm_stack_make_array(placeholder, tvm_stack_make_shape(1, 4, 2520), 0, 3, 0.000000h, 0), tvm_stack_make_array(placeholder, tvm_stack_make_shape(1, 2, 2520), 0, 3, 0.000000h, 0), tvm_stack_make_array(placeholder, tvm_stack_make_shape(1, 2, 2520), 0, 3, 0.000000h, 0), tvm_stack_make_array(placeholder, tvm_stack_make_shape(1, 2520), 0, 2, 0.000000h, 0), tvm_stack_make_array(yolov8_post_process, tvm_stack_make_shape(1, 4, 2520), 0, 3, 0.000000h, 0))
}
