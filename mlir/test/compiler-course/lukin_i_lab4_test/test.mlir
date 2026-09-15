// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/lukin_i_lab4_MLIR%shlibext --pass-pipeline="builtin.module(func-call-counter)" %s | FileCheck %s

module {
  // CHECK-LABEL: func.func @never_called() attributes {call_count = 0 : i32} {
  func.func @never_called() {
    return
  }

  // CHECK-LABEL: func.func @called_once() attributes {call_count = 1 : i32} {
  func.func @called_once() {
    return
  }

  // CHECK-LABEL: func.func @called_multiple() attributes {call_count = 3 : i32} {
  func.func @called_multiple() {
    return
  }

  // CHECK-LABEL: func.func @main() attributes {call_count = 0 : i32} {
  func.func @main() {
    func.call @called_once() : () -> ()
    
    func.call @called_multiple() : () -> ()
    func.call @called_multiple() : () -> ()
    func.call @called_multiple() : () -> ()
    
    return
  }

    // CHECK-LABEL: func.func @recursive() attributes {call_count = 1 : i32} {
  func.func @recursive() {
    func.call @recursive() : () -> ()
    return
  }

  // CHECK-LABEL: func.func @shared_worker() attributes {call_count = 2 : i32} {
  func.func @shared_worker() {
    return
  }

  func.func @caller_one() {
    func.call @shared_worker() : () -> ()
    return
  }

  func.func @caller_two() {
    func.call @shared_worker() : () -> ()
    return
  }

  // CHECK-LABEL: func.func @nested_call() attributes {call_count = 1 : i32}
  func.func @nested_call() {
    return
  }

  func.func @caller_with_regions(%cond: i1) {
    scf.if %cond {
      func.call @nested_call() : () -> ()
    }
    return
  }
}
