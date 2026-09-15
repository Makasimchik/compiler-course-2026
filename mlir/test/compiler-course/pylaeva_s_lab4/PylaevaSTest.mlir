// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/pylaeva_s_lab4_MLIR%shlibext --pass-pipeline="builtin.module(pylaeva_s_lab4_MLIR)" %s | FileCheck %s

// CHECK-LABEL: module
// CHECK-DAG: func.func private @trace_condition_then_begin()
// CHECK-DAG: func.func private @trace_condition_then_end()
// CHECK-DAG: func.func private @trace_condition_else_begin()
// CHECK-DAG: func.func private @trace_condition_else_end()


// ============================================================================
// Тест 1: Базовый scf.if с then и else блоками
// ============================================================================

// CHECK-LABEL: func.func @test_scf_if
// CHECK: scf.if
// CHECK: func.call @trace_condition_then_begin()
// CHECK: arith.constant
// CHECK: func.call @trace_condition_then_end()
// CHECK: } else {
// CHECK: func.call @trace_condition_else_begin()
// CHECK: arith.constant
// CHECK: func.call @trace_condition_else_end()
// CHECK: }
func.func @test_scf_if(%cond: i1) {
  scf.if %cond {
    %c1 = arith.constant 1 : i32
  } else {
    %c2 = arith.constant 2 : i32
  }
  return
}

// ============================================================================
// Тест 2: Базовый affine.if с then и else блоками
// ============================================================================

// CHECK-LABEL: func.func @test_affine_if
// CHECK: affine.if
// CHECK: func.call @trace_condition_then_begin()
// CHECK: arith.constant
// CHECK: func.call @trace_condition_then_end()
// CHECK: } else {
// CHECK: func.call @trace_condition_else_begin()
// CHECK: arith.constant
// CHECK: func.call @trace_condition_else_end()
// CHECK: }
func.func @test_affine_if(%arg0: index) {
  affine.if affine_set<(d0) : (d0 - 1 >= 0)>(%arg0) {
    %c1 = arith.constant 1 : i32
  } else {
    %c2 = arith.constant 2 : i32
  }
  return
}

// ============================================================================
// Тест 3: Вложенные scf.if (проверка корректной вставки в каждый блок)
// ============================================================================

// CHECK-LABEL: func.func @test_nested_scf_if
// CHECK: scf.if
// CHECK: func.call @trace_condition_then_begin()
// CHECK: scf.if
// CHECK: func.call @trace_condition_then_begin()
// CHECK: arith.constant
// CHECK: func.call @trace_condition_then_end()
// CHECK: } else {
// CHECK: func.call @trace_condition_else_begin()
// CHECK: arith.constant
// CHECK: func.call @trace_condition_else_end()
// CHECK: }
// CHECK: func.call @trace_condition_then_end()
// CHECK: } else {
// CHECK: func.call @trace_condition_else_begin()
// CHECK: arith.constant
// CHECK: func.call @trace_condition_else_end()
// CHECK: }
func.func @test_nested_scf_if(%cond1: i1, %cond2: i1) {
  scf.if %cond1 {
    scf.if %cond2 {
      %c1 = arith.constant 1 : i32
    } else {
      %c2 = arith.constant 2 : i32
    }
  } else {
    %c3 = arith.constant 3 : i32
  }
  return
}


// ============================================================================
// Тест 4: Вложенные affine.if (проверка корректной вставки в каждый блок)
// ============================================================================

// CHECK-LABEL: func.func @test_nested_affine_if
// CHECK: affine.if
// CHECK: func.call @trace_condition_then_begin()
// CHECK: affine.if
// CHECK: func.call @trace_condition_then_begin()
// CHECK: arith.constant
// CHECK: func.call @trace_condition_then_end()
// CHECK: } else {
// CHECK: func.call @trace_condition_else_begin()
// CHECK: arith.constant
// CHECK: func.call @trace_condition_else_end()
// CHECK: }
// CHECK: func.call @trace_condition_then_end()
// CHECK: } else {
// CHECK: func.call @trace_condition_else_begin()
// CHECK: arith.constant
// CHECK: func.call @trace_condition_else_end()
// CHECK: }
func.func @test_nested_affine_if(%arg0: index, %arg1: index) {
  affine.if affine_set<(d0) : (d0 - 1 >= 0)>(%arg0) {
    affine.if affine_set<(d0) : (d0 - 2 >= 0)>(%arg1) {
      %c1 = arith.constant 1 : i32
    } else {
      %c2 = arith.constant 2 : i32
    }
  } else {
    %c3 = arith.constant 3 : i32
  }
  return
}



// ============================================================================
// Тест 5: scf.if без else блока
// ============================================================================

// CHECK-LABEL: func.func @test_scf_if_without_else
// CHECK: scf.if
// CHECK: func.call @trace_condition_then_begin()
// CHECK: arith.constant
// CHECK: func.call @trace_condition_then_end()
// CHECK: }
// CHECK-NOT: func.call @trace_condition_else_begin
func.func @test_scf_if_without_else(%cond: i1) {
  scf.if %cond {
    %c1 = arith.constant 1 : i32
  }
  return
}

// ============================================================================
// Тест 6: affine.if без else блока
// ============================================================================

// CHECK-LABEL: func.func @test_affine_if_without_else
// CHECK: affine.if
// CHECK: func.call @trace_condition_then_begin()
// CHECK: arith.constant
// CHECK: func.call @trace_condition_then_end()
// CHECK: }
// CHECK-NOT: func.call @trace_condition_else_begin
func.func @test_affine_if_without_else(%arg0: index) {
  affine.if affine_set<(d0) : (d0 - 1 >= 0)>(%arg0) {
    %c1 = arith.constant 1 : i32
  }
  return
}

// ============================================================================
// Тест 7: Смешанные scf.if и affine.if в одной функции
// ============================================================================

// CHECK-LABEL: func.func @test_mixed_conditions
// CHECK: scf.if
// CHECK: func.call @trace_condition_then_begin()
// CHECK: arith.constant
// CHECK: func.call @trace_condition_then_end()
// CHECK: } else {
// CHECK: func.call @trace_condition_else_begin()
// CHECK: affine.if
// CHECK: func.call @trace_condition_then_begin()
// CHECK: arith.constant
// CHECK: func.call @trace_condition_then_end()
// CHECK: }
// CHECK: func.call @trace_condition_else_end()
// CHECK: }
func.func @test_mixed_conditions(%cond: i1, %idx: index) {
  scf.if %cond {
    %c1 = arith.constant 1 : i32
  } else {
    affine.if affine_set<(d0) : (d0 - 1 >= 0)>(%idx) {
      %c2 = arith.constant 2 : i32
    }
  }
  return
}