// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/spichek_d_lab_4_MLIR%shlibext --pass-pipeline="builtin.module(func.func(spichek-loop-fusion))" %s | FileCheck %s

// CHECK-LABEL: func @test_simple_fusion
// CHECK: scf.for
// CHECK-NOT: scf.for
func.func @test_simple_fusion(%lb: index, %ub: index, %step: index) {
  scf.for %i = %lb to %ub step %step {
    %c1 = arith.constant 1 : i32
  }
  scf.for %j = %lb to %ub step %step {
    %c2 = arith.constant 2 : i32
  }
  return
}

// CHECK-LABEL: func @test_triple_fusion
// CHECK: scf.for
// CHECK-NOT: scf.for
func.func @test_triple_fusion(%lb: index, %ub: index, %step: index) {
  scf.for %i = %lb to %ub step %step {
    %c1 = arith.constant 1 : i32
  }
  scf.for %j = %lb to %ub step %step {
    %c2 = arith.constant 2 : i32
  }
  scf.for %k = %lb to %ub step %step {
    %c3 = arith.constant 3 : i32
  }
  return
}

// CHECK-LABEL: func @test_iter_args_fusion
// CHECK: %[[RES:.*]]:2 = scf.for %{{.*}} = %{{.*}} to %{{.*}} step %{{.*}} iter_args(%[[ARG1:.*]] = %{{.*}}, %[[ARG2:.*]] = %{{.*}})
// CHECK: scf.yield
func.func @test_iter_args_fusion(%lb: index, %ub: index, %step: index, %init1: f32, %init2: f32) -> (f32, f32) {
  %res1 = scf.for %i = %lb to %ub step %step iter_args(%arg1 = %init1) -> (f32) {
    %add = arith.addf %arg1, %arg1 : f32
    scf.yield %add : f32
  }
  %res2 = scf.for %j = %lb to %ub step %step iter_args(%arg2 = %init2) -> (f32) {
    %mul = arith.mulf %arg2, %arg2 : f32
    scf.yield %mul : f32
  }
  return %res1, %res2 : f32, f32
}

// CHECK-LABEL: func @test_different_bounds
// CHECK: scf.for
// CHECK: scf.for
func.func @test_different_bounds(%lb: index, %ub1: index, %ub2: index, %step: index) {
  scf.for %i = %lb to %ub1 step %step {
    %c1 = arith.constant 1 : i32
  }
  scf.for %j = %lb to %ub2 step %step {
    %c2 = arith.constant 2 : i32
  }
  return
}

// CHECK-LABEL: func @test_data_dependency
// CHECK: scf.for
// CHECK: scf.for
func.func @test_data_dependency(%lb: index, %ub: index, %step: index, %init: f32) -> (f32) {
  %res1 = scf.for %i = %lb to %ub step %step iter_args(%arg1 = %init) -> (f32) {
    %add = arith.addf %arg1, %arg1 : f32
    scf.yield %add : f32
  }
  %res2 = scf.for %j = %lb to %ub step %step iter_args(%arg2 = %res1) -> (f32) {
    %mul = arith.mulf %arg2, %arg2 : f32
    scf.yield %mul : f32
  }
  return %res2 : f32
}

// CHECK-LABEL: func @test_intervening_op
// CHECK: scf.for
// CHECK: arith.addi
// CHECK: scf.for
func.func @test_intervening_op(%lb: index, %ub: index, %step: index) {
  scf.for %i = %lb to %ub step %step {
    %c1 = arith.constant 1 : i32
  }
  %c2 = arith.constant 2 : index
  %add = arith.addi %ub, %c2 : index
  scf.for %j = %lb to %ub step %step {
    %c3 = arith.constant 3 : i32
  }
  return
}

// CHECK-LABEL: func @test_different_steps
// CHECK: scf.for
// CHECK: scf.for
func.func @test_different_steps(%lb: index, %ub: index, %step1: index, %step2: index) {
  scf.for %i = %lb to %ub step %step1 {
    %c1 = arith.constant 1 : i32
  }
  scf.for %j = %lb to %ub step %step2 {
    %c2 = arith.constant 2 : i32
  }
  return
}

// CHECK-LABEL: func @test_nested_fusion
// CHECK: scf.for
// CHECK: scf.for
// CHECK-NOT: scf.for
func.func @test_nested_fusion(%lb: index, %ub: index, %step: index) {
  scf.for %k = %lb to %ub step %step {
    scf.for %i = %lb to %ub step %step {
      %c1 = arith.constant 1 : i32
    }
    scf.for %j = %lb to %ub step %step {
      %c2 = arith.constant 2 : i32
    }
  }
  return
}

// CHECK-LABEL: func @test_mixed_iter_args
// CHECK: scf.for %{{.*}} = %{{.*}} to %{{.*}} step %{{.*}} iter_args
// CHECK: scf.yield
func.func @test_mixed_iter_args(%lb: index, %ub: index, %step: index, %init: f32) -> f32 {
  scf.for %i = %lb to %ub step %step {
    %c1 = arith.constant 1 : i32
  }
  %res = scf.for %j = %lb to %ub step %step iter_args(%arg = %init) -> (f32) {
    %add = arith.addf %arg, %arg : f32
    scf.yield %add : f32
  }
  return %res : f32
}