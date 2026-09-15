// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/sannikov_i_lab4_MLIR%shlibext \
// RUN:   --pass-pipeline="builtin.module(sannikov-nesting-depth)" %s | FileCheck %s


// CHECK-LABEL: func.func @just_arithmetic
// CHECK-SAME:  sannikov_nesting_depth = 0 : i64
func.func @just_arithmetic(%x: i32, %y: i32) -> i32 {
  %sum = arith.addi %x, %y : i32
  %result = arith.muli %sum, %y : i32
  return %result : i32
}


// CHECK-LABEL: func.func @one_scf_loop
// CHECK-SAME:  sannikov_nesting_depth = 1 : i64
func.func @one_scf_loop(%lo: index, %hi: index, %step: index) {
  scf.for %i = %lo to %hi step %step {
  }
  return
}


// CHECK-LABEL: func.func @one_affine_loop
// CHECK-SAME:  sannikov_nesting_depth = 1 : i64
func.func @one_affine_loop() {
  affine.for %i = 0 to 16 {
  }
  return
}


// CHECK-LABEL: func.func @two_loops_side_by_side
// CHECK-SAME:  sannikov_nesting_depth = 1 : i64
func.func @two_loops_side_by_side(%lo: index, %hi: index, %step: index) {
  scf.for %i = %lo to %hi step %step {
  }
  affine.for %j = 0 to 8 {
  }
  return
}

// CHECK-LABEL: func.func @one_if
// CHECK-SAME:  sannikov_nesting_depth = 1 : i64
func.func @one_if(%flag: i1) {
  scf.if %flag {
  }
  return
}


// CHECK-LABEL: func.func @for_inside_if
// CHECK-SAME:  sannikov_nesting_depth = 2 : i64
func.func @for_inside_if(%flag: i1, %lo: index, %hi: index, %step: index) {
  scf.if %flag {
    scf.for %i = %lo to %hi step %step {
    }
  }
  return
}


// CHECK-LABEL: func.func @affine_inside_scf
// CHECK-SAME:  sannikov_nesting_depth = 2 : i64
func.func @affine_inside_scf(%lo: index, %hi: index, %step: index) {
  scf.for %i = %lo to %hi step %step {
    affine.for %j = 0 to 8 {
    }
  }
  return
}


// CHECK-LABEL: func.func @while_with_if_inside
// CHECK-SAME:  sannikov_nesting_depth = 2 : i64
func.func @while_with_if_inside(%init: i32, %bound: i32, %flag: i1) {
  %result = scf.while (%cur = %init) : (i32) -> i32 {
    %cond = arith.cmpi slt, %cur, %bound : i32
    scf.condition(%cond) %cur : i32
  } do {
  ^bb0(%cur: i32):
    scf.if %flag {
    }
    scf.yield %cur : i32
  }
  return
}


// CHECK-LABEL: func.func @three_scf_levels
// CHECK-SAME:  sannikov_nesting_depth = 3 : i64
func.func @three_scf_levels(%lo: index, %hi: index, %step: index) {
  scf.for %i = %lo to %hi step %step {
    scf.for %j = %lo to %hi step %step {
      scf.for %k = %lo to %hi step %step {
      }
    }
  }
  return
}

// CHECK-LABEL: func.func @three_mixed_levels
// CHECK-SAME:  sannikov_nesting_depth = 3 : i64
func.func @three_mixed_levels(%lo: index, %hi: index, %step: index, %flag: i1) {
  scf.for %i = %lo to %hi step %step {
    affine.for %j = 0 to 10 {
      scf.if %flag {
      }
    }
  }
  return
}