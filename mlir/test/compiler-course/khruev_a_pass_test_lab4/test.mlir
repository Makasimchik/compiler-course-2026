// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/khruev_a_pass_lab4_MLIR%shlibext --pass-pipeline="builtin.module(func_call_counter)" %s | FileCheck %s

// CHECK-LABEL: func.func @called_twice()
// CHECK-SAME: call_count = 2 : i32
func.func @called_twice() {
  return
}

// CHECK-LABEL: func.func @never_called()
// CHECK-SAME: call_count = 0 : i32
func.func @never_called() {
  return
}

// CHECK-LABEL: llvm.func @llvm_called_once()
// CHECK-SAME: call_count = 1 : i32
llvm.func @llvm_called_once() {
  llvm.return
}

// CHECK-LABEL: func.func @main()
// CHECK-SAME: call_count = 0 : i32
func.func @main() {
  func.call @called_twice() : () -> ()
  func.call @called_twice() : () -> ()
  
  llvm.call @llvm_called_once() : () -> ()
  
  return
}