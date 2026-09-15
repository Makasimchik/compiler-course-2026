// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/lifanov_k_mlir_MLIR%shlibext --pass-pipeline="builtin.module(lifanovk_MLIR)" %s | FileCheck %s

// CHECK-DAG: func.func private @trace_condition_then_begin()
// CHECK-DAG: func.func private @trace_condition_then_end()
// CHECK-DAG: func.func private @trace_condition_else_begin()
// CHECK-DAG: func.func private @trace_condition_else_end()

// CHECK-LABEL: func.func @test_scf_if
func.func @test_scf_if(%arg0: i1) {
  // CHECK: scf.if %arg0 {
  // CHECK-NEXT: func.call @trace_condition_then_begin() : () -> ()
  // CHECK: %{{.*}} = arith.constant 1
  // CHECK: func.call @trace_condition_then_end() : () -> ()
  // CHECK: } else {
  // CHECK-NEXT: func.call @trace_condition_else_begin() : () -> ()
  // CHECK: %{{.*}} = arith.constant 0
  // CHECK: func.call @trace_condition_else_end() : () -> ()
  // CHECK: }
  scf.if %arg0 {
    %0 = arith.constant 1 : i32
  } else {
    %1 = arith.constant 0 : i32
  }
  return
}

// CHECK-LABEL: func.func @test_nested
func.func @test_nested(%arg0: i1, %arg1: i1) {
  // CHECK: scf.if %arg0
  // CHECK:   func.call @trace_condition_then_begin
  // CHECK:   scf.if %arg1
  // CHECK:     func.call @trace_condition_then_begin
  // CHECK:     func.call @trace_condition_then_end
  // CHECK:   func.call @trace_condition_then_end
  scf.if %arg0 {
    scf.if %arg1 {
      %0 = arith.constant 7 : i32
    }
  }
  return
}

#set0 = affine_set<(d0) : (d0 == 0)>

// CHECK-LABEL: func.func @test_affine
func.func @test_affine(%idx : index) {
  // CHECK: affine.if #set
  // CHECK-NEXT: func.call @trace_condition_then_begin() : () -> ()
  // CHECK: func.call @trace_condition_then_end() : () -> ()
  affine.if #set0(%idx) {
    %0 = arith.constant 1 : i32
  }
  return
}

// CHECK-LABEL: func.func @test_empty
func.func @test_empty(%arg0: i1) {
  // CHECK: scf.if %arg0 {
  // CHECK-NEXT: func.call @trace_condition_then_begin() : () -> ()
  // CHECK-NEXT: func.call @trace_condition_then_end() : () -> ()
  scf.if %arg0 {
  }
  return
}