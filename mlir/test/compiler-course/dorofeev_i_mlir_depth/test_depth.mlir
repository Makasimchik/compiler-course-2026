// RUN: mlir-opt --load-pass-plugin=%S/../../../../build/lib/dorofeev_i_mlir_depth_MLIR.so --pass-pipeline="builtin.module(func.func(max-block-depth))" %s | FileCheck %s

// CHECK: func.func @empty_func() attributes {max_block_depth = 0 : i32}
func.func @empty_func() {
  return
}

// CHECK: func.func @single_loop() attributes {max_block_depth = 1 : i32}
func.func @single_loop() {
  %c0 = arith.constant 0 : index
  %c10 = arith.constant 10 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c10 step %c1 {
    // Внутри scf.for создается регион, глубина = 1
  }
  return
}

// Здесь исправили скобки на (%{{.*}})
// CHECK: func.func @nested_structures(%{{.*}}) attributes {max_block_depth = 3 : i32}
func.func @nested_structures(%cond: i1) {
  %c0 = arith.constant 0 : index
  %c10 = arith.constant 10 : index
  %c1 = arith.constant 1 : index
  
  // Уровень 1
  scf.for %i = %c0 to %c10 step %c1 { 
    // Уровень 2
    scf.if %cond {
      // Уровень 3
      affine.for %j = 0 to 10 {
      }
    }
  }
  return
}