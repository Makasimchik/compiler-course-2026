// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/mityaeva_d_lab4_MLIR%shlibext --pass-pipeline='builtin.module(func.func(mityaeva_d_lab4))' %s | FileCheck %s

// CHECK-LABEL: func.func @no_blocks
// CHECK: attributes {max_block_depth = 0 : i64}
func.func @no_blocks() {
  %c0 = arith.constant 0 : index
  return
}

// CHECK-LABEL: func.func @one_for
// CHECK: attributes {max_block_depth = 1 : i64}
func.func @one_for() {
  %c0 = arith.constant 0 : index
  %c10 = arith.constant 10 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %c10 step %c1 {
    %x = arith.addi %i, %i : index
  }
  return
}

// CHECK-LABEL: func.func @nested_for_if
// CHECK: attributes {max_block_depth = 2 : i64}
func.func @nested_for_if() {
  %c0 = arith.constant 0 : index
  %c10 = arith.constant 10 : index
  %c1 = arith.constant 1 : index
  %true = arith.constant true
  scf.for %i = %c0 to %c10 step %c1 {
    scf.if %true {
      %x = arith.addi %i, %i : index
    }
  }
  return
}

// CHECK-LABEL: func.func @sequential_blocks
// CHECK: attributes {max_block_depth = 1 : i64}
func.func @sequential_blocks() {
  %c0 = arith.constant 0 : index
  %c10 = arith.constant 10 : index
  %c1 = arith.constant 1 : index
  %true = arith.constant true
  scf.for %i = %c0 to %c10 step %c1 {
    %x = arith.addi %i, %i : index
  }
  scf.while : () -> () {
    scf.condition(%true)
  } do {
    scf.yield
  }
  return
}

// CHECK-LABEL: func.func @affine_nested
// CHECK: attributes {max_block_depth = 3 : i64}
#set0 = affine_set<(d0) : (d0 >= 0)>
func.func @affine_nested() {
  %c0 = arith.constant 0 : index
  %c10 = arith.constant 10 : index
  %c1 = arith.constant 1 : index
  affine.for %i = 0 to 10 {
    affine.if #set0(%i) {
      scf.for %j = %c0 to %c10 step %c1 {
      }
    }
  }
  return
}