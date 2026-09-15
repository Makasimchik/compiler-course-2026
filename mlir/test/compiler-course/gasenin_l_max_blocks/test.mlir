// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/gasenin_l_max_blocks_MLIR%shlibext \
// RUN:   --pass-pipeline="builtin.module(gasenin_l_max_blocks_MLIR)" %s | FileCheck %s

// ============================================================
// 1. No control flow  →  depth 0
// ============================================================

// CHECK-LABEL: func.func @no_control_flow
// CHECK-SAME:  max_block_depth = 0 : i64
func.func @no_control_flow(%arg0: i32, %arg1: i32) -> i32 {
  %sum = arith.addi %arg0, %arg1 : i32
  return %sum : i32
}

// ============================================================
// 2. Single SCF for  →  depth 1
// ============================================================

// CHECK-LABEL: func.func @scf_single_for
// CHECK-SAME:  max_block_depth = 1 : i64
func.func @scf_single_for(%lb: index, %ub: index, %step: index) {
  scf.for %i = %lb to %ub step %step {
    // empty body
  }
  return
}

// ============================================================
// 3. Single SCF if  →  depth 1
// ============================================================

// CHECK-LABEL: func.func @scf_single_if
// CHECK-SAME:  max_block_depth = 1 : i64
func.func @scf_single_if(%cond: i1) {
  scf.if %cond {
    // then branch
  }
  return
}

// ============================================================
// 4. SCF if with else  →  depth 1
//    (both branches are at the same level — max, not sum)
// ============================================================

// CHECK-LABEL: func.func @scf_if_else
// CHECK-SAME:  max_block_depth = 1 : i64
func.func @scf_if_else(%cond: i1) -> i32 {
  %result = scf.if %cond -> i32 {
    %c1 = arith.constant 1 : i32
    scf.yield %c1 : i32
  } else {
    %c0 = arith.constant 0 : i32
    scf.yield %c0 : i32
  }
  return %result : i32
}

// ============================================================
// 5. SCF while  →  depth 1
// ============================================================

// CHECK-LABEL: func.func @scf_while
// CHECK-SAME:  max_block_depth = 1 : i64
func.func @scf_while(%init: i32, %limit: i32) -> i32 {
  %result = scf.while (%arg = %init) : (i32) -> i32 {
    %cond = arith.cmpi slt, %arg, %limit : i32
    scf.condition(%cond) %arg : i32
  } do {
  ^bb0(%arg: i32):
    %c1 = arith.constant 1 : i32
    %next = arith.addi %arg, %c1 : i32
    scf.yield %next : i32
  }
  return %result : i32
}

// ============================================================
// 6. Single affine.for  →  depth 1
// ============================================================

// CHECK-LABEL: func.func @affine_single_for
// CHECK-SAME:  max_block_depth = 1 : i64
func.func @affine_single_for() {
  affine.for %i = 0 to 10 {
    // empty body
  }
  return
}

// ============================================================
// 7. Single affine.if  →  depth 1
// ============================================================

// CHECK-LABEL: func.func @affine_single_if
// CHECK-SAME:  max_block_depth = 1 : i64
func.func @affine_single_if(%arg0: index) {
  affine.if affine_set<(d0) : (d0 - 1 >= 0)>(%arg0) {
    // then body
  }
  return
}

// ============================================================
// 8. Two sequential (sibling) SCF fors  →  depth 1
//    (siblings add breadth, not depth)
// ============================================================

// CHECK-LABEL: func.func @scf_sequential_fors
// CHECK-SAME:  max_block_depth = 1 : i64
func.func @scf_sequential_fors(%lb: index, %ub: index, %step: index) {
  scf.for %i = %lb to %ub step %step {
  }
  scf.for %j = %lb to %ub step %step {
  }
  return
}

// ============================================================
// 9. Nested SCF for inside SCF for  →  depth 2
// ============================================================

// CHECK-LABEL: func.func @scf_nested_for_depth2
// CHECK-SAME:  max_block_depth = 2 : i64
func.func @scf_nested_for_depth2(%lb: index, %ub: index, %step: index) {
  scf.for %i = %lb to %ub step %step {
    scf.for %j = %lb to %ub step %step {
      // inner body
    }
  }
  return
}

// ============================================================
// 10. SCF if inside SCF for  →  depth 2
// ============================================================

// CHECK-LABEL: func.func @scf_if_in_for
// CHECK-SAME:  max_block_depth = 2 : i64
func.func @scf_if_in_for(%lb: index, %ub: index, %step: index, %cond: i1) {
  scf.for %i = %lb to %ub step %step {
    scf.if %cond {
      // conditional work
    }
  }
  return
}

// ============================================================
// 11. SCF for inside SCF if (then branch)  →  depth 2
// ============================================================

// CHECK-LABEL: func.func @scf_for_in_if
// CHECK-SAME:  max_block_depth = 2 : i64
func.func @scf_for_in_if(%lb: index, %ub: index, %step: index, %cond: i1) {
  scf.if %cond {
    scf.for %i = %lb to %ub step %step {
    }
  }
  return
}

// ============================================================
// 12. SCF for inside the *else* branch  →  depth 2
//     (else contributes the same as then)
// ============================================================

// CHECK-LABEL: func.func @scf_for_in_else
// CHECK-SAME:  max_block_depth = 2 : i64
func.func @scf_for_in_else(%lb: index, %ub: index, %step: index, %cond: i1) {
  scf.if %cond {
    // shallow then
  } else {
    scf.for %i = %lb to %ub step %step {
    }
  }
  return
}

// ============================================================
// 13. Deeper nesting in then vs else  →  max wins  →  depth 3
//     then-branch: for→if (depth 3)
//     else-branch: for→for→if (depth 4)
// ============================================================

// CHECK-LABEL: func.func @scf_asymmetric_branches
// CHECK-SAME:  max_block_depth = 4 : i64
func.func @scf_asymmetric_branches(
    %lb: index, %ub: index, %step: index, %cond: i1) {
  scf.if %cond {
    // depth 2: for → if
    scf.for %i = %lb to %ub step %step {
      scf.if %cond {
      }
    }
  } else {
    // depth 3: for → for → if
    scf.for %i = %lb to %ub step %step {
      scf.for %j = %lb to %ub step %step {
        scf.if %cond {
        }
      }
    }
  }
  return
}

// ============================================================
// 14. Three-level SCF nesting  →  depth 3
// ============================================================

// CHECK-LABEL: func.func @scf_triple_nested
// CHECK-SAME:  max_block_depth = 3 : i64
func.func @scf_triple_nested(%lb: index, %ub: index, %step: index) {
  scf.for %i = %lb to %ub step %step {
    scf.for %j = %lb to %ub step %step {
      scf.for %k = %lb to %ub step %step {
      }
    }
  }
  return
}

// ============================================================
// 15. Affine for nested inside SCF for  →  depth 2
//     (mixed dialect nesting counts normally)
// ============================================================

// CHECK-LABEL: func.func @mixed_scf_affine_depth2
// CHECK-SAME:  max_block_depth = 2 : i64
func.func @mixed_scf_affine_depth2(%lb: index, %ub: index, %step: index) {
  scf.for %i = %lb to %ub step %step {
    affine.for %j = 0 to 16 {
    }
  }
  return
}

// ============================================================
// 16. Affine if nested inside affine for  →  depth 2
// ============================================================

// CHECK-LABEL: func.func @affine_if_in_affine_for
// CHECK-SAME:  max_block_depth = 2 : i64
func.func @affine_if_in_affine_for(%arg0: index) {
  affine.for %i = 0 to 10 {
    affine.if affine_set<(d0) : (d0 - 1 >= 0)>(%i) {
    }
  }
  return
}

// ============================================================
// 17. Three-level mixed nesting  →  depth 3
//     affine.for → SCF for → affine.if
// ============================================================

// CHECK-LABEL: func.func @mixed_triple_nested
// CHECK-SAME:  max_block_depth = 3 : i64
func.func @mixed_triple_nested(%lb: index, %ub: index, %step: index) {
  affine.for %i = 0 to 8 {
    scf.for %j = %lb to %ub step %step {
      affine.for %k = 0 to 4 {
      }
    }
  }
  return
}

// ============================================================
// 18. SCF while with a for inside the do-body  →  depth 2
// ============================================================

// CHECK-LABEL: func.func @scf_while_with_for
// CHECK-SAME:  max_block_depth = 2 : i64
func.func @scf_while_with_for(
    %init: i32, %limit: i32, %lb: index, %ub: index, %step: index) -> i32 {
  %result = scf.while (%arg = %init) : (i32) -> i32 {
    %cond = arith.cmpi slt, %arg, %limit : i32
    scf.condition(%cond) %arg : i32
  } do {
  ^bb0(%arg: i32):
    scf.for %i = %lb to %ub step %step {
      // work inside while body
    }
    %c1 = arith.constant 1 : i32
    %next = arith.addi %arg, %c1 : i32
    scf.yield %next : i32
  }
  return %result : i32
}

// ============================================================
// 19. SCF while with a nested if inside do-body  →  depth 2
// ============================================================

// CHECK-LABEL: func.func @scf_while_with_if
// CHECK-SAME:  max_block_depth = 2 : i64
func.func @scf_while_with_if(%init: i32, %limit: i32, %cond: i1) -> i32 {
  %result = scf.while (%arg = %init) : (i32) -> i32 {
    %loop_cond = arith.cmpi slt, %arg, %limit : i32
    scf.condition(%loop_cond) %arg : i32
  } do {
  ^bb0(%arg: i32):
    scf.if %cond {
    }
    %c1 = arith.constant 1 : i32
    %next = arith.addi %arg, %c1 : i32
    scf.yield %next : i32
  }
  return %result : i32
}

// ============================================================
// 20. Control flow only on one path; other path flat  →  depth is the max
//     Here: a for (depth 1) followed by flat code  →  depth 1
// ============================================================

// CHECK-LABEL: func.func @mixed_flat_and_nested
// CHECK-SAME:  max_block_depth = 1 : i64
func.func @mixed_flat_and_nested(%lb: index, %ub: index, %step: index) -> i32 {
  %c0 = arith.constant 0 : i32
  scf.for %i = %lb to %ub step %step {
  }
  %c1 = arith.constant 1 : i32
  %r = arith.addi %c0, %c1 : i32
  return %r : i32
}

// ============================================================
// 21. Depth-4 pathological nesting  →  depth 4
// ============================================================

// CHECK-LABEL: func.func @deep_nesting_depth4
// CHECK-SAME:  max_block_depth = 4 : i64
func.func @deep_nesting_depth4(
    %lb: index, %ub: index, %step: index, %cond: i1) {
  scf.for %i = %lb to %ub step %step {       // depth 1
    scf.if %cond {                            // depth 2
      affine.for %j = 0 to 8 {              // depth 3
        scf.for %k = %lb to %ub step %step { // depth 4
        }
      }
    }
  }
  return
}
