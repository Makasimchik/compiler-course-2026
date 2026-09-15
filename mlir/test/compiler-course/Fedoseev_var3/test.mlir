// RUN: mlir-opt -allow-unregistered-dialect -load-pass-plugin=%mlir_lib_dir/fedoseev_var3_MLIR%shlibext --pass-pipeline="builtin.module(FedoseevPass4)" %s | FileCheck %s

// CHECK-LABEL: func.func @depth0_no_ops
// CHECK-SAME: max_depth = 0
func.func @depth0_no_ops() -> i32 {
  %c0 = arith.constant 0 : i32
  return %c0 : i32
}

// CHECK-LABEL: func.func @depth1_if
// CHECK-SAME: max_depth = 1
func.func @depth1_if(%cond : i1) {
  scf.if %cond {
    "some.op"() : () -> ()
  }
  return
}

// CHECK-LABEL: func.func @depth1_for
// CHECK-SAME: max_depth = 1
func.func @depth1_for() {
  %c0 = arith.constant 0 : index
  %c10 = arith.constant 10 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c10 step %c1 {
    "some.op"() : () -> ()
  }
  return
}

// CHECK-LABEL: func.func @depth1_while
// CHECK-SAME: max_depth = 1
func.func @depth1_while(%n : i32) -> i32 {
  %c0 = arith.constant 0 : i32
  %res = scf.while (%arg = %n) : (i32) -> i32 {
    %cmp = arith.cmpi sgt, %arg, %c0 : i32
    scf.condition(%cmp) %arg : i32
  } do {
  ^bb0(%arg: i32):
    %dec = arith.subi %arg, %c0 : i32
    scf.yield %dec : i32
  }
  return %res : i32
}

// CHECK-LABEL: func.func @depth1_sequential_ifs
// CHECK-SAME: max_depth = 1
func.func @depth1_sequential_ifs(%cond1 : i1, %cond2 : i1) {
  scf.if %cond1 {
    "some.op"() : () -> ()
  }
  scf.if %cond2 {
    "some.op"() : () -> ()
  }
  return
}

// CHECK-LABEL: func.func @depth2_if_inside_if
// CHECK-SAME: max_depth = 2
func.func @depth2_if_inside_if(%cond1 : i1, %cond2 : i1) {
  scf.if %cond1 {
    scf.if %cond2 {
      "some.op"() : () -> ()
    }
  }
  return
}

// CHECK-LABEL: func.func @depth2_for_inside_for
// CHECK-SAME: max_depth = 2
func.func @depth2_for_inside_for() {
  %c0 = arith.constant 0 : index
  %c5 = arith.constant 5 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c5 step %c1 {
    scf.for %j = %c0 to %c5 step %c1 {
      "some.op"() : () -> ()
    }
  }
  return
}

// CHECK-LABEL: func.func @depth2_while_inside_if
// CHECK-SAME: max_depth = 2
func.func @depth2_while_inside_if(%cond : i1, %n : i32) -> i32 {
  %c0 = arith.constant 0 : i32
  %res = scf.if %cond -> i32 {
    %val = scf.while (%arg = %n) : (i32) -> i32 {
      %cmp = arith.cmpi sgt, %arg, %c0 : i32
      scf.condition(%cmp) %arg : i32
    } do {
    ^bb0(%arg: i32):
      %dec = arith.subi %arg, %c0 : i32
      scf.yield %dec : i32
    }
    scf.yield %val : i32
  } else {
    scf.yield %c0 : i32
  }
  return %res : i32
}

// CHECK-LABEL: func.func @depth2_if_inside_for
// CHECK-SAME: max_depth = 2
func.func @depth2_if_inside_for(%cond : i1) {
  %c0 = arith.constant 0 : index
  %c5 = arith.constant 5 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c5 step %c1 {
    scf.if %cond {
      "some.op"() : () -> ()
    }
  }
  return
}

// CHECK-LABEL: func.func @depth2_for_inside_if
// CHECK-SAME: max_depth = 2
func.func @depth2_for_inside_if(%cond : i1) {
  %c0 = arith.constant 0 : index
  %c5 = arith.constant 5 : index
  %c1 = arith.constant 1 : index
  scf.if %cond {
    scf.for %i = %c0 to %c5 step %c1 {
      "some.op"() : () -> ()
    }
  }
  return
}
// CHECK-LABEL: func.func @depth3_if_if_if
// CHECK-SAME: max_depth = 3
func.func @depth3_if_if_if(%c1 : i1, %c2 : i1, %c3 : i1) {
  scf.if %c1 {
    scf.if %c2 {
      scf.if %c3 {
        "some.op"() : () -> ()
      }
    }
  }
  return
}

// CHECK-LABEL: func.func @depth3_for_for_for
// CHECK-SAME: max_depth = 3
func.func @depth3_for_for_for() {
  %c0 = arith.constant 0 : index
  %c3 = arith.constant 3 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c3 step %c1 {
    scf.for %j = %c0 to %c3 step %c1 {
      scf.for %k = %c0 to %c3 step %c1 {
        "some.op"() : () -> ()
      }
    }
  }
  return
}

// CHECK-LABEL: func.func @depth3_mixed
// CHECK-SAME: max_depth = 3
func.func @depth3_mixed(%cond : i1) {
  %c0 = arith.constant 0 : index
  %c3 = arith.constant 3 : index
  %c1 = arith.constant 1 : index
  scf.if %cond {
    scf.for %i = %c0 to %c3 step %c1 {
      scf.if %cond {
        "some.op"() : () -> ()
      }
    }
  }
  return
}

// CHECK-LABEL: func.func @depth4_if_for_if_while
// CHECK-SAME: max_depth = 4
func.func @depth4_if_for_if_while(%cond1 : i1, %cond2 : i1, %n : i32) -> i32 {
  %c0 = arith.constant 0 : index
  %c3 = arith.constant 3 : index
  %c1 = arith.constant 1 : index
  %c0_i32 = arith.constant 0 : i32
  %res = scf.if %cond1 -> i32 {
    %sum = scf.for %i = %c0 to %c3 step %c1 iter_args(%acc = %c0_i32) -> i32 {
      %val = scf.if %cond2 -> i32 {
        %w = scf.while (%arg = %n) : (i32) -> i32 {
          %cmp = arith.cmpi sgt, %arg, %c0_i32 : i32
          scf.condition(%cmp) %arg : i32
        } do {
        ^bb0(%arg: i32):
          %dec = arith.subi %arg, %c0_i32 : i32
          scf.yield %dec : i32
        }
        scf.yield %w : i32
      } else {
        scf.yield %c0_i32 : i32
      }
      %new_acc = arith.addi %acc, %val : i32
      scf.yield %new_acc : i32
    }
    scf.yield %sum : i32
  } else {
    scf.yield %c0_i32 : i32
  }
  return %res : i32
}

// CHECK-LABEL: func.func @ignore_non_control_flow
// CHECK-SAME: max_depth = 1
func.func @ignore_non_control_flow(%cond : i1) -> i32 {
  %c1 = arith.constant 1 : i32
  %c2 = arith.constant 2 : i32
  %sum = arith.addi %c1, %c2 : i32
  scf.if %cond {
    "some.op"() : () -> ()
  }
  return %sum : i32
}