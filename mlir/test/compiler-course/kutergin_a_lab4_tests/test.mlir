// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/kutergin_a_lab4_MLIR%shlibext --pass-pipeline="builtin.module(count-function-calls)" %s | FileCheck %s

// CHECK: func.func @target_multi_call
// CHECK-SAME: kutergin_call_count = 3 : i64
func.func @target_multi_call() {
  return
}

// CHECK: func.func @target_recursive
// CHECK-SAME: kutergin_call_count = 1 : i64
func.func @target_recursive() {
  call @target_recursive() : () -> ()
  return
}

// CHECK: func.func @target_in_loop
// CHECK-SAME: kutergin_call_count = 1 : i64
func.func @target_in_loop() {
  return
}

// CHECK: func.func @unused_func
// CHECK-SAME: kutergin_call_count = 0 : i64
func.func @unused_func() {
  return
}

func.func @main_caller() {
  call @target_multi_call() : () -> ()
  call @target_multi_call() : () -> ()

  "test.dummy_region"() ({
    call @target_multi_call() : () -> ()
    call @target_in_loop() : () -> ()
    "test.terminator"() : () -> ()
  }) : () -> ()

  return
}