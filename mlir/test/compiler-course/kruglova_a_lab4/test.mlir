// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/kruglova_a_lab4_MLIR.so --pass-pipeline="builtin.module(func-call-counter)" %s 2>&1 | FileCheck %s

// тест  проверка вывода общего количества операций в консоль
// CHECK: Count operations: 20

// тест 1 проверка базового подсчета 
// CHECK: func.func @target_simple() attributes {call_count = 1 : i32}
func.func @target_simple() {
  return
}

// CHECK: func.func @unused_func() attributes {call_count = 0 : i32}
func.func @unused_func() {
  return
}

// тест 2 множественные вызовы одной функции из разных мест
// CHECK: func.func @multi_target() attributes {call_count = 3 : i32}
func.func @multi_target() {
  return
}

// тест 3 рекурсия (функция вызывает сама себя)
// CHECK: func.func @recursive_func() attributes {call_count = 1 : i32}
func.func @recursive_func() {
  func.call @recursive_func() : () -> ()
  return
}

// тест 4 проверка вызовов внутри регионов (цикл scf.for)
// CHECK: func.func @scf_caller(%{{.*}}: index, %{{.*}}: index, %{{.*}}: index) attributes {call_count = 0 : i32}
func.func @scf_caller(%arg0: index, %arg1: index, %arg2: index) {
  scf.for %i = %arg0 to %arg1 step %arg2 {
    func.call @target_simple() : () -> ()
  }
  return
}

// тест 5 вызывающая функция (main)
// CHECK: func.func @main() attributes {call_count = 0 : i32}
func.func @main() {
  func.call @multi_target() : () -> ()
  func.call @multi_target() : () -> ()
  func.call @multi_target() : () -> ()
  return
}

