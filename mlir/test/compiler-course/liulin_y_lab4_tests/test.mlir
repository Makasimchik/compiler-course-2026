// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/liulin_y_lab4_MLIR%shlibext --pass-pipeline="builtin.module(liulin_loop_fuse)" %s | FileCheck %s

// CHECK-LABEL: func @test_simple_fusion
// CHECK: scf.for
// CHECK-NEXT: arith.addi
// CHECK-NEXT: arith.muli
// CHECK-NOT: scf.for
// CHECK: return
func.func @test_simple_fusion(%lb: index, %ub: index, %step: index, %a: i32, %b: i32) {
  scf.for %i = %lb to %ub step %step {
    %0 = arith.addi %a, %b : i32
  }
  scf.for %j = %lb to %ub step %step {
    %1 = arith.muli %a, %b : i32
  }
  return
}

// CHECK-LABEL: func @test_three_loops
// CHECK: scf.for
// CHECK-NEXT: arith.addi
// CHECK-NEXT: arith.muli
// CHECK-NEXT: arith.subi
// CHECK-NOT: scf.for
// CHECK: return
func.func @test_three_loops(%lb: index, %ub: index, %step: index, %a: i32, %b: i32) {
  scf.for %i = %lb to %ub step %step { %0 = arith.addi %a, %b : i32 }
  scf.for %j = %lb to %ub step %step { %1 = arith.muli %a, %b : i32 }
  scf.for %k = %lb to %ub step %step { %2 = arith.subi %a, %b : i32 }
  return
}

// CHECK-LABEL: func @test_iter_args
// CHECK-SAME: (%[[LB:.*]]: index, %[[UB:.*]]: index, %[[STEP:.*]]: index, %[[INIT1:.*]]: i32, %[[INIT2:.*]]: f32)
// CHECK: %[[RES:.*]]:2 = scf.for %[[IV:.*]] = %[[LB]] to %[[UB]] step %[[STEP]] iter_args(%[[ARG1:.*]] = %[[INIT1]], %[[ARG2:.*]] = %[[INIT2]]) -> (i32, f32) {
// CHECK-NEXT:   %[[ADD:.*]] = arith.addi %[[ARG1]], %[[INIT1]] : i32
// CHECK-NEXT:   %[[ADD_F:.*]] = arith.addf %[[ARG2]], %[[INIT2]] : f32
// CHECK-NEXT:   scf.yield %[[ADD]], %[[ADD_F]] : i32, f32
// CHECK-NEXT: }
// CHECK-NEXT: return %[[RES]]#0, %[[RES]]#1 : i32, f32
func.func @test_iter_args(%lb: index, %ub: index, %step: index, %init1: i32, %init2: f32) -> (i32, f32) {
  %res1 = scf.for %i = %lb to %ub step %step iter_args(%arg1 = %init1) -> (i32) {
    %0 = arith.addi %arg1, %init1 : i32
    scf.yield %0 : i32
  }
  %res2 = scf.for %j = %lb to %ub step %step iter_args(%arg2 = %init2) -> (f32) {
    %1 = arith.addf %arg2, %init2 : f32
    scf.yield %1 : f32
  }
  return %res1, %res2 : i32, f32
}

// CHECK-LABEL: func @test_mixed_args
// CHECK: %[[RES:.*]] = scf.for {{.*}} iter_args
// CHECK-NEXT: arith.addi
// CHECK-NEXT: arith.muli
// CHECK-NEXT: scf.yield
// CHECK: return %[[RES]]
func.func @test_mixed_args(%lb: index, %ub: index, %step: index, %init: i32, %val: i32) -> i32 {
  %res = scf.for %i = %lb to %ub step %step iter_args(%arg1 = %init) -> (i32) {
    %0 = arith.addi %arg1, %val : i32
    scf.yield %0 : i32
  }
  scf.for %j = %lb to %ub step %step {
    %1 = arith.muli %val, %val : i32
  }
  return %res : i32
}

// CHECK-LABEL: func @test_nested
// CHECK: scf.for
// CHECK:   scf.for
// CHECK-NEXT: arith.addi
// CHECK-NEXT: arith.muli
// CHECK-NOT: scf.for
// CHECK: return
func.func @test_nested(%lb: index, %ub: index, %step: index, %val: i32) {
  scf.for %outer = %lb to %ub step %step {
    scf.for %i = %lb to %ub step %step {
      %0 = arith.addi %val, %val : i32
    }
    scf.for %j = %lb to %ub step %step {
      %1 = arith.muli %val, %val : i32
    }
  }
  return
}

// CHECK-LABEL: func @test_diff_bounds
// CHECK: scf.for
// CHECK: scf.for
func.func @test_diff_bounds(%lb1: index, %lb2: index, %ub: index, %step: index) {
  scf.for %i = %lb1 to %ub step %step { }
  scf.for %j = %lb2 to %ub step %step { }
  return
}

// CHECK-LABEL: func @test_diff_step
// CHECK: scf.for
// CHECK: scf.for
func.func @test_diff_step(%lb: index, %ub: index, %step1: index, %step2: index) {
  scf.for %i = %lb to %ub step %step1 { }
  scf.for %j = %lb to %ub step %step2 { }
  return
}

// CHECK-LABEL: func @test_data_dependency
// CHECK: %[[RES1:.*]] = scf.for
// CHECK: scf.for {{.*}} iter_args({{.*}} = %[[RES1]])
func.func @test_data_dependency(%lb: index, %ub: index, %step: index, %init: i32) -> i32 {
  %res1 = scf.for %i = %lb to %ub step %step iter_args(%arg1 = %init) -> (i32) {
    %0 = arith.addi %arg1, %init : i32
    scf.yield %0 : i32
  }
  %res2 = scf.for %j = %lb to %ub step %step iter_args(%arg2 = %res1) -> (i32) {
    %1 = arith.muli %arg2, %init : i32
    scf.yield %1 : i32
  }
  return %res2 : i32
}

// CHECK-LABEL: func @test_intervening_op
// CHECK: scf.for
// CHECK: arith.addi
// CHECK: scf.for
func.func @test_intervening_op(%lb: index, %ub: index, %step: index, %val: i32) {
  scf.for %i = %lb to %ub step %step { }
  %0 = arith.addi %val, %val : i32
  scf.for %j = %lb to %ub step %step { }
  return
}