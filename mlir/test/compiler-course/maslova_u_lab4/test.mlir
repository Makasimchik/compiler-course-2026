// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/maslova_u_lab4_MLIR%shlibext --pass-pipeline="builtin.module(maslova-condition-tracer)" %s | FileCheck %s

func.func @test_scf_full(%arg0: i1) {
  // CHECK-LABEL: func.func @test_scf_full
  scf.if %arg0 {
    // CHECK: func.call @trace_condition_then_begin()
    // CHECK: func.call @trace_condition_then_end()
    %c1 = arith.constant 1 : i32
  } else {
    // CHECK: func.call @trace_condition_else_begin()
    // CHECK: func.call @trace_condition_else_end()
    %c0 = arith.constant 0 : i32
  }
  return
}

func.func @test_scf_no_else(%arg0: i1) {
  // CHECK-LABEL: func.func @test_scf_no_else
  scf.if %arg0 {
    // CHECK: func.call @trace_condition_then_begin()
    // CHECK: func.call @trace_condition_then_end()
  }
  // CHECK-NOT: func.call @trace_condition_else_begin()
  return
}

func.func @test_affine_full(%arg0: index) {
  // CHECK-LABEL: func.func @test_affine_full
  affine.if affine_set<(d0) : (d0 >= 0)>(%arg0) {
    // CHECK: func.call @trace_condition_then_begin()
    // CHECK: func.call @trace_condition_then_end()
  } else {
    // CHECK: func.call @trace_condition_else_begin()
    // CHECK: func.call @trace_condition_else_end()
  }
  return
}

func.func @test_nested(%arg0: i1, %arg1: i1) {
  // CHECK-LABEL: func.func @test_nested
  scf.if %arg0 {
    // CHECK: func.call @trace_condition_then_begin()
    scf.if %arg1 {
      // CHECK: func.call @trace_condition_then_begin()
      // CHECK: func.call @trace_condition_then_end()
    }
    // CHECK: func.call @trace_condition_then_end()
  }
  return
}

func.func @test_empty(%arg0: i1) {
  // CHECK-LABEL: func.func @test_empty
  scf.if %arg0 {
    // CHECK: func.call @trace_condition_then_begin()
    // CHECK-NEXT: func.call @trace_condition_then_end()
  }
  return
}

func.func @test_loop_if(%arg0: index, %arg1: index, %arg2: index) {
  // CHECK-LABEL: func.func @test_loop_if
  scf.for %i = %arg0 to %arg1 step %arg2 {
    %cond = arith.constant 1 : i1
    scf.if %cond {
      // CHECK: func.call @trace_condition_then_begin()
      // CHECK: func.call @trace_condition_then_end()
    }
  }
  return
}
