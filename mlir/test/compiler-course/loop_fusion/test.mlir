// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/loop_fusion_MLIR%shlibext --pass-pipeline="builtin.module(adjacent-scf-loop-fusion)" %s | FileCheck %s

func.func @fuse_independent_loops(%src: memref<10xf32>, %tmp: memref<10xf32>, %dst: memref<10xf32>, %out: memref<10xf32>) {
  %c0 = arith.constant 0 : index
  %c10 = arith.constant 10 : index
  %c1 = arith.constant 1 : index

  // CHECK-LABEL: func.func @fuse_independent_loops
  // CHECK-SAME: (%[[SRC:.*]]: memref<10xf32>, %[[TMP:.*]]: memref<10xf32>, %[[DST:.*]]: memref<10xf32>, %[[OUT:.*]]: memref<10xf32>)
  // CHECK: scf.for %[[IV:.*]] = %c0 to %c10 step %c1 {
  // CHECK-NEXT: %[[V1:.*]] = memref.load %[[SRC]][%[[IV]]] : memref<10xf32>
  // CHECK-NEXT: memref.store %[[V1]], %[[TMP]][%[[IV]]] : memref<10xf32>
  // CHECK-NEXT: %[[V2:.*]] = memref.load %[[DST]][%[[IV]]] : memref<10xf32>
  // CHECK-NEXT: memref.store %[[V2]], %[[OUT]][%[[IV]]] : memref<10xf32>
  // CHECK-NEXT: }
  // CHECK-NOT: scf.for
  scf.for %i = %c0 to %c10 step %c1 {
    %v1 = memref.load %src[%i] : memref<10xf32>
    memref.store %v1, %tmp[%i] : memref<10xf32>
  }
  scf.for %j = %c0 to %c10 step %c1 {
    %v2 = memref.load %dst[%j] : memref<10xf32>
    memref.store %v2, %out[%j] : memref<10xf32>
  }
  return
}

func.func @fuse_three_loops(%a: memref<10xf32>, %b: memref<10xf32>, %c: memref<10xf32>) {
  %c0a = arith.constant 0 : index
  %c10a = arith.constant 10 : index
  %c1a = arith.constant 1 : index
  %c0b = arith.constant 0 : index
  %c10b = arith.constant 10 : index
  %c1b = arith.constant 1 : index
  %f = arith.constant 1.0 : f32

  // CHECK-LABEL: func.func @fuse_three_loops
  // CHECK: scf.for
  // CHECK-NOT: scf.for
  scf.for %i = %c0a to %c10a step %c1a {
    memref.store %f, %a[%i] : memref<10xf32>
  }
  scf.for %j = %c0b to %c10b step %c1b {
    memref.store %f, %b[%j] : memref<10xf32>
  }
  scf.for %k = %c0a to %c10a step %c1a {
    memref.store %f, %c[%k] : memref<10xf32>
  }
  return
}

func.func @do_not_fuse_different_bounds(%arg: memref<10xf32>) {
  %c0 = arith.constant 0 : index
  %c5 = arith.constant 5 : index
  %c10 = arith.constant 10 : index
  %c1 = arith.constant 1 : index
  %f = arith.constant 2.0 : f32

  // CHECK-LABEL: func.func @do_not_fuse_different_bounds
  // CHECK: scf.for
  // CHECK: scf.for
  scf.for %i = %c0 to %c5 step %c1 {
    memref.store %f, %arg[%i] : memref<10xf32>
  }
  scf.for %j = %c0 to %c10 step %c1 {
    memref.store %f, %arg[%j] : memref<10xf32>
  }
  return
}

func.func @do_not_fuse_flow_dependency(%arg0: memref<10xf32>, %arg1: memref<10xf32>) {
  %c0 = arith.constant 0 : index
  %c10 = arith.constant 10 : index
  %c1 = arith.constant 1 : index

  // CHECK-LABEL: func.func @do_not_fuse_flow_dependency
  // CHECK: scf.for
  // CHECK: scf.for
  scf.for %i = %c0 to %c10 step %c1 {
    %v = memref.load %arg0[%i] : memref<10xf32>
    memref.store %v, %arg1[%i] : memref<10xf32>
  }
  scf.for %j = %c0 to %c10 step %c1 {
    %v = memref.load %arg1[%j] : memref<10xf32>
    memref.store %v, %arg0[%j] : memref<10xf32>
  }
  return
}

func.func @do_not_fuse_iter_args(%arg: memref<10xf32>) -> f32 {
  %c0 = arith.constant 0 : index
  %c10 = arith.constant 10 : index
  %c1 = arith.constant 1 : index
  %zero = arith.constant 0.0 : f32

  // CHECK-LABEL: func.func @do_not_fuse_iter_args
  // CHECK: scf.for
  // CHECK: scf.for
  %sum = scf.for %i = %c0 to %c10 step %c1 iter_args(%acc = %zero) -> (f32) {
    %v = memref.load %arg[%i] : memref<10xf32>
    %next = arith.addf %acc, %v : f32
    scf.yield %next : f32
  }
  scf.for %j = %c0 to %c10 step %c1 {
    memref.store %sum, %arg[%j] : memref<10xf32>
  }
  return %sum : f32
}