// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/morozova_s_lab4_MLIR%shlibext --pass-pipeline="builtin.module(call-counter)" %s | FileCheck %s

// CHECK-LABEL: func.func @simple_function
// CHECK-SAME: {call_count = 3 : i64}
func.func @simple_function() {
  return
}

// CHECK-LABEL: func.func @function_called_once
// CHECK-SAME: {call_count = 1 : i64}
func.func @function_called_once() {
  return
}

// CHECK-LABEL: func.func @function_never_called
// CHECK-SAME: {call_count = 0 : i64}
func.func @function_never_called() {
  return
}

// CHECK-LABEL: func.func @recursive_function
// CHECK-SAME: {call_count = 2 : i64}
func.func @recursive_function(%arg: index) -> index {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %cmp = arith.cmpi eq, %arg, %c0 : index
  %result = scf.if %cmp -> index {
    scf.yield %c0 : index
  } else {
    %dec = arith.subi %arg, %c1 : index
    %res = func.call @recursive_function(%dec) : (index) -> index
    scf.yield %res : index
  }
  func.return %result : index
}

// CHECK-LABEL: func.func @called_in_loop
// CHECK-SAME: {call_count = 1 : i64}
func.func @called_in_loop() {
  return
}

// CHECK-LABEL: func.func @called_in_conditional
// CHECK-SAME: {call_count = 2 : i64}
func.func @called_in_conditional() {
  return
}

// CHECK-LABEL: func.func @called_from_nested_scopes
// CHECK-SAME: {call_count = 2 : i64}
func.func @called_from_nested_scopes() {
  return
}

// CHECK-LABEL: module @nested_module {
// CHECK-LABEL: func.func @inner_function
// CHECK-SAME: {call_count = 2 : i64}
// CHECK-LABEL: func.func @outer_function
// CHECK-SAME: {call_count = 0 : i64}
module @nested_module {
  func.func @inner_function() {
    return
  }

  func.func @outer_function() {
    func.call @inner_function() : () -> ()
    func.call @inner_function() : () -> ()
    return
  }
}

// CHECK-LABEL: func.func @with_arguments
// CHECK-SAME: {call_count = 2 : i64}
func.func @with_arguments(%a: index, %b: f32) -> index {
  %c0 = arith.constant 0 : index
  return %c0 : index
}

// CHECK-LABEL: func.func @main
// CHECK-SAME: {call_count = 0 : i64}
func.func @main() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c2 = arith.constant 2 : index
  %c5 = arith.constant 5 : index
  %c10 = arith.constant 10 : index
  %cf1 = arith.constant 1.0 : f32

  func.call @simple_function() : () -> ()
  func.call @simple_function() : () -> ()
  func.call @simple_function() : () -> ()

  func.call @function_called_once() : () -> ()

  scf.for %i = %c0 to %c10 step %c1 {
    func.call @called_in_loop() : () -> ()
  }

  scf.for %i = %c0 to %c5 step %c1 {
    %cond = arith.cmpi eq, %i, %c1 : index
    scf.if %cond {
      func.call @called_in_conditional() : () -> ()
    } else {
      func.call @called_in_conditional() : () -> ()
    }
  }

  scf.for %i = %c0 to %c5 step %c1 {
    scf.for %j = %c0 to %c2 step %c1 {
      func.call @called_from_nested_scopes() : () -> ()
    }
    func.call @called_from_nested_scopes() : () -> ()
  }

  %res = func.call @recursive_function(%c5) : (index) -> index

  func.call @with_arguments(%c1, %cf1) : (index, f32) -> index
  func.call @with_arguments(%c5, %cf1) : (index, f32) -> index

  return
}
