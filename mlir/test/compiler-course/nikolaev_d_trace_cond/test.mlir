// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/nikolaev_d_trace_cond_MLIR%shlibext \
// RUN: --pass-pipeline="builtin.module(nikolaev_d_trace_cond_MLIR)" %s | FileCheck %s

module {
  // CHECK-DAG: func.func private @trace_condition_then_begin()
  // CHECK-DAG: func.func private @trace_condition_then_end()
  // CHECK-DAG: func.func private @trace_condition_else_begin()
  // CHECK-DAG: func.func private @trace_condition_else_end()

  // CHECK-LABEL: func.func @test_scf_if
  func.func @test_scf_if(%cond: i1) {
    // CHECK: scf.if %arg0 {
    scf.if %cond {
      // CHECK-NEXT: func.call @trace_condition_then_begin()
      %c1 = arith.constant 1 : i32
      // CHECK: func.call @trace_condition_then_end()
      // CHECK-NEXT: }
      scf.yield
    } else {
      // CHECK-NEXT: func.call @trace_condition_else_begin()
      %c0 = arith.constant 0 : i32
      // CHECK: func.call @trace_condition_else_end()
      // CHECK-NEXT: }
      scf.yield
    }
    return
  }

  // CHECK-LABEL: func.func @test_affine_if
  func.func @test_affine_if(%arg0: index) {
    affine.if affine_set<(d0) : (d0 >= 0)>(%arg0) {
      // CHECK: func.call @trace_condition_then_begin()
      %c1 = arith.constant 1 : i32
      // CHECK: func.call @trace_condition_then_end()
      affine.yield
    } else {
      // CHECK: func.call @trace_condition_else_begin()
      %c0 = arith.constant 0 : i32
      // CHECK: func.call @trace_condition_else_end()
      affine.yield
    }
    return
  }

  // CHECK-LABEL: func.func @test_scf_for
  func.func @test_scf_for(%arg0: index, %arg1: index, %arg2: index) {
    scf.for %i = %arg0 to %arg1 step %arg2 {
      // CHECK: func.call @trace_condition_then_begin()
      %c1 = arith.constant 1 : i32
      // CHECK: func.call @trace_condition_then_end()
      scf.yield
    }
    return
  }

  // CHECK-LABEL: func.func @test_scf_while
  func.func @test_scf_while(%arg0: i32) {
    %res = scf.while (%arg1 = %arg0) : (i32) -> i32 {
      // CHECK: func.call @trace_condition_then_begin()
      %cond = arith.cmpi "eq", %arg1, %arg1 : i32
      // CHECK: func.call @trace_condition_then_end()
      scf.condition(%cond) %arg1 : i32
    } do {
    ^bb0(%arg2: i32):
      // CHECK: func.call @trace_condition_then_begin()
      %c1 = arith.constant 1 : i32
      // CHECK: func.call @trace_condition_then_end()
      scf.yield %arg2 : i32
    }
    return
  }

  // CHECK-LABEL: func.func @test_nested_structures
  func.func @test_nested_structures(%cond: i1) {
    %c0 = arith.constant 0 : index
    %c10 = arith.constant 10 : index
    %c1 = arith.constant 1 : index
    
    scf.for %i = %c0 to %c10 step %c1 {
      // CHECK: func.call @trace_condition_then_begin()
      
      scf.if %cond {
        // CHECK: func.call @trace_condition_then_begin()
        %c1_i32 = arith.constant 1 : i32
        // CHECK: func.call @trace_condition_then_end()
        scf.yield
      }
      
      // CHECK: func.call @trace_condition_then_end()
      scf.yield
    }
    return
  }

  // CHECK-LABEL: func.func @test_if_no_else
  func.func @test_if_no_else(%cond: i1) {
    // CHECK: scf.if %arg0 {
    scf.if %cond {
      // CHECK-NEXT: func.call @trace_condition_then_begin()
      %0 = arith.constant 42 : i32
      // CHECK: func.call @trace_condition_then_end()
      scf.yield
    }
    // CHECK-NOT: func.call @trace_condition_else_begin()
    return
  }

  // CHECK-LABEL: func.func @test_deep_nesting
  func.func @test_deep_nesting(%c1: i1, %c2: i1) {
    scf.if %c1 {
      // CHECK: func.call @trace_condition_then_begin()
      scf.if %c2 {
        // CHECK: func.call @trace_condition_then_begin()
        %0 = arith.constant 1 : i32
        // CHECK: func.call @trace_condition_then_end()
        scf.yield
      }
      // CHECK: func.call @trace_condition_then_end()
      scf.yield
    }
    return
  }

  // CHECK-LABEL: func.func @test_empty_blocks
  func.func @test_empty_blocks(%cond: i1) {
    scf.if %cond {
      // CHECK: func.call @trace_condition_then_begin()
      // CHECK-NEXT: func.call @trace_condition_then_end()
      // CHECK-NEXT: }
      scf.yield
    }
    return
  }
}