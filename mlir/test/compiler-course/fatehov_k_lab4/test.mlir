// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/fatehov_k_lab4_MLIR%shlibext --pass-pipeline="builtin.module(fatehov_k_lab4)" %s | FileCheck %s

// CHECK: module {
// CHECK-DAG: func.func private @trace_condition_then_begin()
// CHECK-DAG: func.func private @trace_condition_then_end()
// CHECK-DAG: func.func private @trace_condition_else_begin()
// CHECK-DAG: func.func private @trace_condition_else_end()

// CHECK-LABEL: func.func @scf_with_else
// CHECK: scf.if %arg0 {
// CHECK: func.call @trace_condition_then_begin() : () -> ()
// CHECK: arith.constant 0 : i32
// CHECK: func.call @trace_condition_then_end() : () -> ()
// CHECK: } else {
// CHECK: func.call @trace_condition_else_begin() : () -> ()
// CHECK: arith.constant 1 : i32
// CHECK: func.call @trace_condition_else_end() : () -> ()
// CHECK: return

// CHECK-LABEL: func.func @scf_without_else
// CHECK: scf.if %arg0 {
// CHECK: func.call @trace_condition_then_begin() : () -> ()
// CHECK: arith.constant 42 : i32
// CHECK: func.call @trace_condition_then_end() : () -> ()
// CHECK-NOT: trace_condition_else_begin
// CHECK-NOT: trace_condition_else_end
// CHECK: return

// CHECK-LABEL: func.func @scf_result_if
// CHECK: scf.if %arg0 -> (i32) {
// CHECK: func.call @trace_condition_then_begin() : () -> ()
// CHECK: arith.constant 10 : i32
// CHECK: func.call @trace_condition_then_end() : () -> ()
// CHECK: scf.yield
// CHECK: } else {
// CHECK: func.call @trace_condition_else_begin() : () -> ()
// CHECK: arith.constant 20 : i32
// CHECK: func.call @trace_condition_else_end() : () -> ()
// CHECK: scf.yield
// CHECK: return

// CHECK-LABEL: func.func @nested_scf_if
// CHECK: scf.if %arg0 {
// CHECK: func.call @trace_condition_then_begin() : () -> ()
// CHECK: scf.if %arg1 {
// CHECK: func.call @trace_condition_then_begin() : () -> ()
// CHECK: arith.constant 100 : i32
// CHECK: func.call @trace_condition_then_end() : () -> ()
// CHECK: } else {
// CHECK: func.call @trace_condition_else_begin() : () -> ()
// CHECK: arith.constant 200 : i32
// CHECK: func.call @trace_condition_else_end() : () -> ()
// CHECK: func.call @trace_condition_then_end() : () -> ()
// CHECK: } else {
// CHECK: func.call @trace_condition_else_begin() : () -> ()
// CHECK: arith.constant 300 : i32
// CHECK: func.call @trace_condition_else_end() : () -> ()
// CHECK: return

// CHECK-LABEL: func.func @affine_with_else
// CHECK: affine.if
// CHECK: func.call @trace_condition_then_begin() : () -> ()
// CHECK: arith.constant 5 : i32
// CHECK: func.call @trace_condition_then_end() : () -> ()
// CHECK: } else {
// CHECK: func.call @trace_condition_else_begin() : () -> ()
// CHECK: arith.constant 6 : i32
// CHECK: func.call @trace_condition_else_end() : () -> ()
// CHECK: return

#set0 = affine_set<(d0) : (d0 - 10 >= 0)>

module {
  func.func @scf_with_else(%arg0: i1) {
    scf.if %arg0 {
      %c0 = arith.constant 0 : i32
    } else {
      %c1 = arith.constant 1 : i32
    }
    return
  }

  func.func @scf_without_else(%arg0: i1) {
    scf.if %arg0 {
      %c42 = arith.constant 42 : i32
    }
    return
  }

  func.func @scf_result_if(%arg0: i1) -> i32 {
    %0 = scf.if %arg0 -> (i32) {
      %c10 = arith.constant 10 : i32
      scf.yield %c10 : i32
    } else {
      %c20 = arith.constant 20 : i32
      scf.yield %c20 : i32
    }
    return %0 : i32
  }

  func.func @nested_scf_if(%arg0: i1, %arg1: i1) {
    scf.if %arg0 {
      scf.if %arg1 {
        %c100 = arith.constant 100 : i32
      } else {
        %c200 = arith.constant 200 : i32
      }
    } else {
      %c300 = arith.constant 300 : i32
    }
    return
  }

  func.func @affine_with_else(%arg0: index) {
    affine.if #set0(%arg0) {
      %c5 = arith.constant 5 : i32
    } else {
      %c6 = arith.constant 6 : i32
    }
    return
  }
}
