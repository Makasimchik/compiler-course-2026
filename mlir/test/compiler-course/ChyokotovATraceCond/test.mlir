// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/ChyokotovATraceCond_MLIR%shlibext \
// RUN:   --pass-pipeline="builtin.module(chyokotov_trace_cond_MLIR)" %s | FileCheck %s

// CHECK-LABEL: func.func @test_scf_if
func.func @test_scf_if(%cond: i1) {
  // CHECK: scf.if
  scf.if %cond {
    // CHECK-NEXT: func.call @trace_condition_then_begin()
    %c1 = arith.constant 1 : i32
    // CHECK: func.call @trace_condition_then_end()
    // CHECK-NEXT: } else {
  } else {
    // CHECK-NEXT: func.call @trace_condition_else_begin()
    %c2 = arith.constant 2 : i32
    // CHECK: func.call @trace_condition_else_end()
    // CHECK-NEXT: }
  }
  return
}

// CHECK-LABEL: func.func @test_scf_for
func.func @test_scf_for(%lb: index, %ub: index, %step: index) {
  // CHECK: scf.for
  scf.for %i = %lb to %ub step %step {
    // CHECK-NEXT: func.call @trace_condition_then_begin()
    %c1 = arith.constant 1 : i32
    // CHECK: func.call @trace_condition_then_end()
    // CHECK-NEXT: }
  }
  return
}

// CHECK-LABEL: func.func @test_scf_for_with_args
func.func @test_scf_for_with_args(%lb: index, %ub: index, %step: index, %init: i32) -> i32 {
  // CHECK: scf.for
  %result = scf.for %i = %lb to %ub step %step iter_args(%arg = %init) -> i32 {
    // CHECK-NEXT: func.call @trace_condition_then_begin()
    %c1 = arith.constant 2 : i32
    %sum = arith.addi %arg, %c1 : i32
    // CHECK: func.call @trace_condition_then_end()
    scf.yield %sum : i32
  }
  return %result : i32
}

// CHECK-LABEL: func.func @test_affine_if
func.func @test_affine_if(%arg0: index) {
  // CHECK: affine.if
  affine.if affine_set<(d0) : (d0 - 1 >= 0)>(%arg0) {
    // CHECK-NEXT: func.call @trace_condition_then_begin()
    %c1 = arith.constant 1 : i32
    // CHECK: func.call @trace_condition_then_end()
    // CHECK-NEXT: } else {
  } else {
    // CHECK-NEXT: func.call @trace_condition_else_begin()
    %c2 = arith.constant 2 : i32
    // CHECK: func.call @trace_condition_else_end()
    // CHECK-NEXT: }
  }
  return
}

// CHECK-LABEL: func.func @test_affine_if_nested
func.func @test_affine_if_nested(%arg0: index, %arg1: index) {
  // CHECK: affine.if
  affine.if affine_set<(d0, d1) : (d0 - 1 >= 0, d1 - 2 >= 0)>(%arg0, %arg1) {
    // CHECK-NEXT: func.call @trace_condition_then_begin()
    %c1 = arith.constant 1 : i32
    // CHECK: func.call @trace_condition_then_end()
    // CHECK-NEXT: } else {
  } else {
    // CHECK-NEXT: func.call @trace_condition_else_begin()
    %c2 = arith.constant 2 : i32
    // CHECK: func.call @trace_condition_else_end()
    // CHECK-NEXT: }
  }
  return
}

// CHECK-LABEL: func.func @test_multiple_conditions
func.func @test_multiple_conditions(%cond1: i1, %cond2: i1) {
  // CHECK: scf.if
  scf.if %cond1 {
    // CHECK-NEXT: func.call @trace_condition_then_begin()
    // CHECK: scf.if
    scf.if %cond2 {
      // CHECK-NEXT: func.call @trace_condition_then_begin()
      %c1 = arith.constant 1 : i32
      // CHECK: func.call @trace_condition_then_end()
      // CHECK-NEXT: } else {
    } else {
      // CHECK-NEXT: func.call @trace_condition_else_begin()
      %c2 = arith.constant 2 : i32
      // CHECK: func.call @trace_condition_else_end()
      // CHECK-NEXT: }
    }
    // CHECK: func.call @trace_condition_then_end()
    // CHECK-NEXT: } else {
  } else {
    // CHECK-NEXT: func.call @trace_condition_else_begin()
    %c3 = arith.constant 3 : i32
    // CHECK: func.call @trace_condition_else_end()
    // CHECK-NEXT: }
  }
  return
}

// CHECK-LABEL: func.func @test_affine_if_no_else
func.func @test_affine_if_no_else(%arg0: index) {
  // CHECK: affine.if
  affine.if affine_set<(d0) : (d0 - 1 >= 0)>(%arg0) {
    // CHECK-NEXT: func.call @trace_condition_then_begin()
    %c1 = arith.constant 1 : i32
    // CHECK: func.call @trace_condition_then_end()
    // CHECK-NEXT: }
  }
  return
}

// CHECK-LABEL: func.func @test_scf_if_no_else
func.func @test_scf_if_no_else(%cond: i1) {
  // CHECK: scf.if
  scf.if %cond {
    // CHECK-NEXT: func.call @trace_condition_then_begin()
    %c1 = arith.constant 1 : i32
    // CHECK: func.call @trace_condition_then_end()
    // CHECK-NEXT: }
  }
  return
}