produce yolov8_post_process {
  // attr [0] extern_scope = 0
  tvm_call_packed("tvm_contrib_yolov8_post_process", tvm_stack_make_array(placeholder, tvm_stack_make_shape(1, 4, 2520), 0, 3, 0.000000h, 0), tvm_stack_make_array(placeholder, tvm_stack_make_shape(1, 2, 2520), 0, 3, 0.000000h, 0), tvm_stack_make_array(placeholder, tvm_stack_make_shape(1, 2, 2520), 0, 3, 0.000000h, 0), tvm_stack_make_array(placeholder, tvm_stack_make_shape(1, 2520), 0, 2, 0.000000h, 0), tvm_stack_make_array(yolov8_post_process, tvm_stack_make_shape(1, 4, 2520), 0, 3, 0.000000h, 0))
}
