// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/guseva_a_lab4_MLIR%shlibext --pass-pipeline="builtin.module(FuseAdjacentScfForPass)" %s | FileCheck %s

func.func private @first_loop(%i : index)
func.func private @second_loop(%i : index)
func.func private @third_loop(%i : index)

// CHECK-LABEL: func.func @fuse_simple_loops()
// CHECK: %[[C0:.*]] = arith.constant 0 : index
// CHECK: %[[C8:.*]] = arith.constant 8 : index
// CHECK: %[[C1:.*]] = arith.constant 1 : index
// CHECK: scf.for %[[IV:.*]] = %[[C0]] to %[[C8]] step %[[C1]] {
// CHECK: func.call @first_loop(%[[IV]]) : (index) -> ()
// CHECK: func.call @second_loop(%[[IV]]) : (index) -> ()
// CHECK-NOT: scf.for
// CHECK: return
func.func @fuse_simple_loops() {
  %c0 = arith.constant 0 : index
  %c8 = arith.constant 8 : index
  %c1 = arith.constant 1 : index

  scf.for %i = %c0 to %c8 step %c1 {
    func.call @first_loop(%i) : (index) -> ()
  }

  scf.for %j = %c0 to %c8 step %c1 {
    func.call @second_loop(%j) : (index) -> ()
  }

  return
}

// CHECK-LABEL: func.func @fuse_same_constant_values()
// CHECK: scf.for %[[IV:.*]] = {{.*}} to {{.*}} step {{.*}} {
// CHECK: func.call @first_loop(%[[IV]]) : (index) -> ()
// CHECK: func.call @second_loop(%[[IV]]) : (index) -> ()
// CHECK-NOT: scf.for
// CHECK: return
func.func @fuse_same_constant_values() {
  %c0_a = arith.constant 0 : index
  %c8_a = arith.constant 8 : index
  %c1_a = arith.constant 1 : index

  %c0_b = arith.constant 0 : index
  %c8_b = arith.constant 8 : index
  %c1_b = arith.constant 1 : index

  scf.for %i = %c0_a to %c8_a step %c1_a {
    func.call @first_loop(%i) : (index) -> ()
  }

  scf.for %j = %c0_b to %c8_b step %c1_b {
    func.call @second_loop(%j) : (index) -> ()
  }

  return
}

// CHECK-LABEL: func.func @do_not_fuse_different_bounds()
// CHECK: scf.for
// CHECK: func.call @first_loop
// CHECK: scf.for
// CHECK: func.call @second_loop
// CHECK: return
func.func @do_not_fuse_different_bounds() {
  %c0 = arith.constant 0 : index
  %c8 = arith.constant 8 : index
  %c16 = arith.constant 16 : index
  %c1 = arith.constant 1 : index

  scf.for %i = %c0 to %c8 step %c1 {
    func.call @first_loop(%i) : (index) -> ()
  }

  scf.for %j = %c0 to %c16 step %c1 {
    func.call @second_loop(%j) : (index) -> ()
  }

  return
}

// CHECK-LABEL: func.func @do_not_fuse_loop_carried_values()
// CHECK: scf.for {{.*}} iter_args
// CHECK: scf.for
// CHECK: func.call @second_loop
// CHECK: return
func.func @do_not_fuse_loop_carried_values() -> i32 {
  %c0_i = arith.constant 0 : i32
  %c0 = arith.constant 0 : index
  %c8 = arith.constant 8 : index
  %c1 = arith.constant 1 : index

  %res = scf.for %i = %c0 to %c8 step %c1 iter_args(%acc = %c0_i) -> i32 {
    %one = arith.constant 1 : i32
    %next = arith.addi %acc, %one : i32
    scf.yield %next : i32
  }

  scf.for %j = %c0 to %c8 step %c1 {
    func.call @second_loop(%j) : (index) -> ()
  }

  return %res : i32
}

// CHECK-LABEL: func.func @do_not_fuse_same_memref_access
// CHECK: scf.for
// CHECK: memref.load
// CHECK: memref.store
// CHECK: scf.for
// CHECK: memref.load
// CHECK: memref.store
// CHECK: return
func.func @do_not_fuse_same_memref_access(%A : memref<16xi32>) {
  %c0 = arith.constant 0 : index
  %c8 = arith.constant 8 : index
  %c1 = arith.constant 1 : index

  scf.for %i = %c0 to %c8 step %c1 {
    %x = memref.load %A[%i] : memref<16xi32>
    memref.store %x, %A[%i] : memref<16xi32>
  }

  scf.for %j = %c0 to %c8 step %c1 {
    %y = memref.load %A[%j] : memref<16xi32>
    memref.store %y, %A[%j] : memref<16xi32>
  }

  return
}

// CHECK-LABEL: func.func @fuse_three_adjacent_loops()
// CHECK: scf.for %[[IV:.*]] = {{.*}} to {{.*}} step {{.*}} {
// CHECK: func.call @first_loop(%[[IV]]) : (index) -> ()
// CHECK: func.call @second_loop(%[[IV]]) : (index) -> ()
// CHECK: func.call @third_loop(%[[IV]]) : (index) -> ()
// CHECK-NOT: scf.for
// CHECK: return
func.func @fuse_three_adjacent_loops() {
  %c0 = arith.constant 0 : index
  %c8 = arith.constant 8 : index
  %c1 = arith.constant 1 : index

  scf.for %i = %c0 to %c8 step %c1 {
    func.call @first_loop(%i) : (index) -> ()
  }

  scf.for %j = %c0 to %c8 step %c1 {
    func.call @second_loop(%j) : (index) -> ()
  }

  scf.for %k = %c0 to %c8 step %c1 {
    func.call @third_loop(%k) : (index) -> ()
  }

  return
}

// CHECK-LABEL: func.func @do_not_fuse_different_step()
// CHECK: scf.for
// CHECK: func.call @first_loop
// CHECK: scf.for
// CHECK: func.call @second_loop
func.func @do_not_fuse_different_step() {
  %c0 = arith.constant 0 : index
  %c8 = arith.constant 8 : index
  %c1 = arith.constant 1 : index
  %c2 = arith.constant 2 : index

  scf.for %i = %c0 to %c8 step %c1 {
    func.call @first_loop(%i) : (index) -> ()
  }

  scf.for %j = %c0 to %c8 step %c2 {
    func.call @second_loop(%j) : (index) -> ()
  }

  return
}

// CHECK-LABEL: func.func @do_not_fuse_different_lower_bound()
// CHECK: scf.for
// CHECK: func.call @first_loop
// CHECK: scf.for
// CHECK: func.call @second_loop
func.func @do_not_fuse_different_lower_bound() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c8 = arith.constant 8 : index

  scf.for %i = %c0 to %c8 step %c1 {
    func.call @first_loop(%i) : (index) -> ()
  }

  scf.for %j = %c1 to %c8 step %c1 {
    func.call @second_loop(%j) : (index) -> ()
  }

  return
}
