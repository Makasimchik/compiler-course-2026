// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/gonozov_l_lab_4_MLIR%shlibext --pass-pipeline="builtin.module(FunctionCallCounterPass)" %s | FileCheck %s




module {

  // CHECK: func.func @isEven(%arg0: i32) -> i1 attributes {call_count = 0 : i32}
  // Функция, которая никогда не вызывается
  func.func @isEven(%arg0: i32) -> i1 {
    %0 = arith.constant 1 : i32
    %1 = arith.constant 0 : i32
    %2 = arith.andi %arg0, %0 : i32
    %3 = arith.cmpi eq, %2, %1 : i32
    func.return %3 : i1
  }

  // CHECK: func.func @helper(%arg0: i32) -> i32 attributes {call_count = 2 : i32} 
  // Вспомогательная функция, которая будет вызвана дважды
  func.func @helper(%arg0: i32) -> i32 {
    %0 = arith.constant 1 : i32
    %1 = arith.addi %arg0, %0 : i32
    func.return %1 : i32
  }
  
  // CHECK: func.func @main() attributes {call_count = 0 : i32}
  // Функция, которая дважды вызывает helper
  func.func @main() {
    %0 = arith.constant 5 : i32
    %1 = func.call @helper(%0) : (i32) -> i32
    %2 = arith.constant 10 : i32
    %3 = func.call @helper(%2) : (i32) -> i32
    func.return
  }

  // CHECK: func.func @foo() attributes {call_count = 3 : i32}
  func.func @foo() {
    func.return
  }
  
  // CHECK: func.func @bar() attributes {call_count = 2 : i32}
  func.func @bar() {
    // bar вызывает foo дважды
    func.call @foo() : () -> ()
    func.call @foo() : () -> ()
    func.return
  }

  // CHECK: func.func @baz() attributes {call_count = 1 : i32}
  func.func @baz() {
    // baz вызывает foo один раз и bar один раз
    func.call @foo() : () -> ()
    func.call @bar() : () -> ()
    func.return
  }

  // CHECK: func.func @fi() attributes {call_count = 0 : i32}
  func.func @fi() {
    // fi вызывает bar один раз и baz один раз
    func.call @bar() : () -> ()
    func.call @baz() : () -> ()
    func.return
  }

  // CHECK: func.func @rec() attributes {call_count = 1 : i32}
  func.func @rec() {
    // вызывает сама себя один раз
    func.call @rec() : () -> ()
    return
  }
}
