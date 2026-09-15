// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/krykov_e_max_depth_MLIR%shlibext \
// RUN:   --pass-pipeline="builtin.module(max-nesting-depth)" %s | FileCheck %s

// CHECK: Function 'flat': max nesting depth = 0
// CHECK: Function 'one_level': max nesting depth = 1
// CHECK: Function 'two_levels': max nesting depth = 2
// CHECK: Function 'three_levels': max nesting depth = 3
// CHECK: Function 'parallel_branches': max nesting depth = 2

// CHECK: @flat{{.*}}max_nesting_depth = 0
// CHECK: @one_level{{.*}}max_nesting_depth = 1
// CHECK: @two_levels{{.*}}max_nesting_depth = 2
// CHECK: @three_levels{{.*}}max_nesting_depth = 3
// CHECK: @parallel_branches{{.*}}max_nesting_depth = 2

// depth 0.
func.func @flat(%arg0: i32) -> i32 {
  %c1 = arith.constant 1 : i32
  %res = arith.addi %arg0, %c1 : i32
  return %res : i32
}

// depth 1
func.func @one_level(%arg0: index, %arg1: index, %arg2: index) {
  scf.for %i = %arg0 to %arg1 step %arg2 {
    %c0 = arith.constant 0 : index
  }
  return
}

// depth 2
func.func @two_levels(%cond: i1, %arg0: index, %arg1: index, %arg2: index) {
  scf.if %cond {
    scf.for %i = %arg0 to %arg1 step %arg2 {
      %c0 = arith.constant 0 : index
    }
  }
  return
}

// depth 3
func.func @three_levels(%cond: i1, %arg0: index, %arg1: index, %arg2: index) {
  scf.for %i = %arg0 to %arg1 step %arg2 {
    scf.if %cond {
      scf.for %j = %arg0 to %arg1 step %arg2 {
        %c1 = arith.constant 1 : index
      }
    }
  }
  return
}

func.func @parallel_branches(%cond: i1, %arg0: index, %arg1: index, %arg2: index) {
  scf.for %i = %arg0 to %arg1 step %arg2 {
    scf.if %cond {
      %c0 = arith.constant 0 : index
    }
  }
  scf.for %j = %arg0 to %arg1 step %arg2 {
    scf.if %cond {
      %c1 = arith.constant 1 : index
    }
  }
  return
}
