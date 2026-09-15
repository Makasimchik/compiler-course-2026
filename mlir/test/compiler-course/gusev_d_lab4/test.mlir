// RUN: mlir-opt --allow-unregistered-dialect -load-pass-plugin=%mlir_lib_dir/gusev_d_lab4_MLIR%shlibext --pass-pipeline="builtin.module(gusev-d-lab4)" %s | FileCheck %s
// UNSUPPORTED: system-windows

// CHECK-DAG: func.func private @trace_condition_then_begin()
// CHECK-DAG: func.func private @trace_condition_then_end()
// CHECK-DAG: func.func private @trace_condition_else_begin()
// CHECK-DAG: func.func private @trace_condition_else_end()

#set = affine_set<(d0) : (d0 - 4 >= 0)>

func.func @instrument_scf_if(%cond: i1, %value: i32) -> i32 {
  %result = scf.if %cond -> i32 {
    %then = arith.addi %value, %value : i32
    scf.yield %then : i32
  } else {
    %else = arith.subi %value, %value : i32
    scf.yield %else : i32
  }
  return %result : i32
}

// CHECK-LABEL: func.func @instrument_scf_if
// CHECK: scf.if
// CHECK-NEXT: func.call @trace_condition_then_begin()
// CHECK-NEXT: {{.*}} = arith.addi
// CHECK-NEXT: func.call @trace_condition_then_end()
// CHECK-NEXT: scf.yield
// CHECK: func.call @trace_condition_else_begin()
// CHECK-NEXT: {{.*}} = arith.subi
// CHECK-NEXT: func.call @trace_condition_else_end()
// CHECK-NEXT: scf.yield

func.func @instrument_affine_if(%index: index) -> index {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %result = affine.if #set(%index) -> index {
    "test.then"() : () -> ()
    affine.yield %c0 : index
  } else {
    "test.else"() : () -> ()
    affine.yield %c1 : index
  }
  return %result : index
}

// CHECK-LABEL: func.func @instrument_affine_if
// CHECK: affine.if
// CHECK-NEXT: func.call @trace_condition_then_begin()
// CHECK-NEXT: "test.then"
// CHECK-NEXT: func.call @trace_condition_then_end()
// CHECK-NEXT: affine.yield
// CHECK: func.call @trace_condition_else_begin()
// CHECK-NEXT: "test.else"
// CHECK-NEXT: func.call @trace_condition_else_end()
// CHECK-NEXT: affine.yield

func.func @instrument_nested_if(%outer: i1, %inner: i1) -> i32 {
  %c0 = arith.constant 0 : i32
  %c1 = arith.constant 1 : i32
  %c2 = arith.constant 2 : i32
  %result = scf.if %outer -> i32 {
    %inner_result = scf.if %inner -> i32 {
      "test.inner_then"() : () -> ()
      scf.yield %c1 : i32
    } else {
      "test.inner_else"() : () -> ()
      scf.yield %c2 : i32
    }
    scf.yield %inner_result : i32
  } else {
    "test.outer_else"() : () -> ()
    scf.yield %c0 : i32
  }
  return %result : i32
}

// CHECK-LABEL: func.func @instrument_nested_if
// CHECK: scf.if
// CHECK-NEXT: func.call @trace_condition_then_begin()
// CHECK-NEXT: {{.*}} = scf.if
// CHECK-NEXT: func.call @trace_condition_then_begin()
// CHECK-NEXT: "test.inner_then"
// CHECK-NEXT: func.call @trace_condition_then_end()
// CHECK-NEXT: scf.yield
// CHECK: func.call @trace_condition_else_begin()
// CHECK-NEXT: "test.inner_else"
// CHECK-NEXT: func.call @trace_condition_else_end()
// CHECK-NEXT: scf.yield
// CHECK: func.call @trace_condition_then_end()
// CHECK-NEXT: scf.yield
// CHECK: func.call @trace_condition_else_begin()
// CHECK-NEXT: "test.outer_else"
// CHECK-NEXT: func.call @trace_condition_else_end()
// CHECK-NEXT: scf.yield
