// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/romanova_v_func_calls_counter_MLIR%shlibext --pass-pipeline="builtin.module(func-calls-counter)" %s | FileCheck %s


module {
  // CHECK-LABEL: func.func @foo
  // CHECK: attributes {call_count = 2 : i32}
  func.func @foo() -> i32 {
    %0 = arith.constant 42 : i32
    func.return %0 : i32
  }

  // CHECK-LABEL: func.func @bar
  // CHECK: attributes {call_count = 2 : i32}
  func.func @bar(%arg: i32) -> i32 {
    %c1 = arith.constant 1 : i32
    %c0 = arith.constant 0 : i32
    %cmp = arith.cmpi eq, %arg, %c0 : i32
    cf.cond_br %cmp, ^bb1, ^bb2
    
  ^bb1:
    func.return %c1 : i32
    
  ^bb2:
    %sub = arith.subi %arg, %c1 : i32
    %call1 = func.call @bar(%sub) : (i32) -> i32  
    %call2 = func.call @foo() : () -> i32        
    %res = arith.addi %call1, %call2 : i32
    func.return %res : i32
  }

  // CHECK-LABEL: func.func @main
  // CHECK: attributes {call_count = 0 : i32}
  func.func @main() -> i32 {
    %c1 = arith.constant 1 : i32
    %c2 = arith.constant 2 : i32
    
    %3 = func.call @foo() : () -> i32      
    %4 = func.call @bar(%c1) : (i32) -> i32 
    %5 = func.call @bar(%c2) : (i32) -> i32 

    %6 = arith.addi %3, %4 : i32
    %7 = arith.addi %6, %5 : i32
    
    func.return %7 : i32
  }
}
