// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/volkov_a_lab4_MLIR%shlibext \
// RUN:   --pass-pipeline="builtin.module(volkov-call-counter)" %s \
// RUN:   | FileCheck %s

module {

  // функция не вызывается нигде — call_count = 0
  // CHECK: @check_odd
  // CHECK-SAME: call_count = 0 : i32
  func.func @check_odd(%arg0: i32) -> i1 {
    %c2  = arith.constant 2 : i32
    %c0  = arith.constant 0 : i32
    %rem = arith.remsi %arg0, %c2 : i32
    %res = arith.cmpi ne, %rem, %c0 : i32
    func.return %res : i1
  }

  // вызывается 2 раза из @entry_point — call_count = 2
  // CHECK: @add_two
  // CHECK-SAME: call_count = 2 : i32
  func.func @add_two(%arg0: i32) -> i32 {
    %c2  = arith.constant 2 : i32
    %sum = arith.addi %arg0, %c2 : i32
    func.return %sum : i32
  }

  // точка входа, никем не вызывается — call_count = 0
  // CHECK: @entry_point
  // CHECK-SAME: call_count = 0 : i32
  func.func @entry_point() {
    %c10 = arith.constant 10 : i32
    %0   = func.call @add_two(%c10) : (i32) -> i32
    %c20 = arith.constant 20 : i32
    %1   = func.call @add_two(%c20) : (i32) -> i32
    func.return
  }

  // вызывается из @func_b (2 раза) и @func_c (1 раз) — call_count = 3
  // CHECK: @func_a
  // CHECK-SAME: call_count = 3 : i32
  func.func @func_a() {
    func.return
  }

  // вызывается из @func_c (1 раз) и @func_d (1 раз) — call_count = 2
  // CHECK: @func_b
  // CHECK-SAME: call_count = 2 : i32
  func.func @func_b() {
    func.call @func_a() : () -> ()
    func.call @func_a() : () -> ()
    func.return
  }

  // вызывается из @func_d (1 раз) — call_count = 1
  // CHECK: @func_c
  // CHECK-SAME: call_count = 1 : i32
  func.func @func_c() {
    func.call @func_a() : () -> ()
    func.call @func_b() : () -> ()
    func.return
  }

  // не вызывается нигде — call_count = 0
  // CHECK: @func_d
  // CHECK-SAME: call_count = 0 : i32
  func.func @func_d() {
    func.call @func_b() : () -> ()
    func.call @func_c() : () -> ()
    func.return
  }

  // рекурсивная функция: пасс считает статически —
  // внутри тела ровно один CallOp на саму себя → call_count = 1
  // CHECK: @recursive_func
  // CHECK-SAME: call_count = 1 : i32
  func.func @recursive_func() {
    func.call @recursive_func() : () -> ()
    func.return
  }

}