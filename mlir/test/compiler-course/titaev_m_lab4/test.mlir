// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/titaev_m_lab4_MLIR%shlibext
// --pass-pipeline="builtin.module(trace-condition)" %s | FileCheck %s

// CHECK-LABEL: func.func @test_scf_if_else
func.func @test_scf_if_else(% cond : i1) {
  // CHECK: scf.if %arg0 {
  // CHECK-NEXT: call @trace_condition_then_begin()
  // CHECK-NEXT: "test.op1"()
  // CHECK-NEXT: call @trace_condition_then_end()
  // CHECK-NEXT: scf.yield
  // CHECK-NEXT: } else {
  // CHECK-NEXT: call @trace_condition_else_begin()
  // CHECK-NEXT: "test.op2"()
  // CHECK-NEXT: call @trace_condition_else_end()
  // CHECK-NEXT: scf.yield
  // CHECK-NEXT: }
  scf.if % cond { "test.op1"() : ()->() scf.yield }
  else {
    "test.op2"() : ()->() scf.yield
  }
  return
}

// CHECK-LABEL: func.func @test_scf_if_no_else
func.func @test_scf_if_no_else(% cond : i1) {
  // CHECK: scf.if %arg0 {
  // CHECK-NEXT: call @trace_condition_then_begin()
  // CHECK-NEXT: "test.op"()
  // CHECK-NEXT: call @trace_condition_then_end()
  // CHECK-NEXT: }
  // CHECK-NOT: call @trace_condition_else_begin
  scf.if % cond { "test.op"() : ()->() scf.yield }
  return
}

// CHECK-LABEL: func.func @test_affine_if_else
func.func @test_affine_if_else(% idx : index) {
  // CHECK: affine.if {{.*}} {
  // CHECK-NEXT: call @trace_condition_then_begin()
  // CHECK-NEXT: "test.op_a"()
  // CHECK-NEXT: call @trace_condition_then_end()
  // CHECK-NEXT: } else {
  // CHECK-NEXT: call @trace_condition_else_begin()
  // CHECK-NEXT: "test.op_b"()
  // CHECK-NEXT: call @trace_condition_else_end()
  // CHECK-NEXT: }
  affine.if affine_set<(d0) : (d0 == 0)>(% idx) { "test.op_a"() : ()->() }
  else {
    "test.op_b"() : ()->()
  }
  return
}

// CHECK-LABEL: func.func @test_nested_ifs
func.func @test_nested_ifs(% c1 : i1, % c2 : i1) {
  // CHECK: scf.if %arg0
  // CHECK:   call @trace_condition_then_begin
  // CHECK:   scf.if %arg1
  // CHECK:     call @trace_condition_then_begin
  // CHECK:     call @trace_condition_then_end
  // CHECK:   call @trace_condition_then_end
  scf.if % c1 { scf.if % c2{"test.inner"() : ()->() scf.yield} scf.yield }
  return
}

// CHECK-LABEL: func.func @test_empty_blocks
func.func @test_empty_blocks(% cond : i1) 
  // CHECK: scf.if %arg0 {
  // CHECK-NEXT: call @trace_condition_then_begin()
  // CHECK-NEXT: call @trace_condition_then_end()
  // CHECK-NEXT: scf.yield
  scf.if % cond { scf.yield }
  return
}

// CHECK: func.func private @trace_condition_then_begin()
// CHECK: func.func private @trace_condition_then_end()
// CHECK: func.func private @trace_condition_else_begin()
// CHECK: func.func private @trace_condition_else_end()