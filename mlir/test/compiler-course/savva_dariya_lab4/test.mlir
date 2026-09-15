// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir//SavvaDariyaCopyToLoopPass_Savva_Dariya_FIIT1_MLIR%shlibext \
// RUN: --pass-pipeline="builtin.module(savva-copy-to-loop)" %s | FileCheck %s


// CHECK-LABEL: func.func @copy_1d

// CHECK-DAG: %[[C0:[a-zA-Z0-9_]+]] = arith.constant 0 : index
// CHECK-DAG: %[[C1:[a-zA-Z0-9_]+]] = arith.constant 1 : index
// CHECK-DAG: %[[C4:[a-zA-Z0-9_]+]] = arith.constant 4 : index

// CHECK: scf.for %[[I:[a-zA-Z0-9_]+]] = %[[C0]] to %[[C4]] step %[[C1]] {
// CHECK: memref.load %arg0[%[[I]]]
// CHECK: memref.store
func.func @copy_1d(%A: memref<4xi32>, %B: memref<4xi32>) {
  memref.copy %A, %B : memref<4xi32> to memref<4xi32>
  return
}




// CHECK-LABEL: func.func @copy_2d

// CHECK-DAG: %[[C0:[a-zA-Z0-9_]+]] = arith.constant 0 : index
// CHECK-DAG: %[[C1:[a-zA-Z0-9_]+]] = arith.constant 1 : index
// CHECK-DAG: %[[C2:[a-zA-Z0-9_]+]] = arith.constant 2 : index
// CHECK-DAG: %[[C3:[a-zA-Z0-9_]+]] = arith.constant 3 : index

// CHECK: scf.for %[[I:[a-zA-Z0-9_]+]] = %[[C0]] to %[[C2]] step %[[C1]] {
// CHECK: scf.for %[[J:[a-zA-Z0-9_]+]] = %[[C0]] to %[[C3]] step %[[C1]] {
// CHECK: memref.load %arg0[%[[I]], %[[J]]]
// CHECK: memref.store
func.func @copy_2d(%A: memref<2x3xi32>, %B: memref<2x3xi32>) {
  memref.copy %A, %B : memref<2x3xi32> to memref<2x3xi32>
  return
}




// CHECK-LABEL: func.func @copy_dynamic

// CHECK-DAG: %[[C0:[a-zA-Z0-9_]+]] = arith.constant 0 : index
// CHECK-DAG: %[[C1:[a-zA-Z0-9_]+]] = arith.constant 1 : index

// CHECK: %[[DIM:[a-zA-Z0-9_]+]] = memref.dim %arg0
// CHECK: scf.for %[[I:[a-zA-Z0-9_]+]] = %[[C0]] to %[[DIM]] step %[[C1]] {
// CHECK: memref.load %arg0[%[[I]]]
// CHECK: memref.store
func.func @copy_dynamic(%A: memref<?xi32>, %B: memref<?xi32>) {
  memref.copy %A, %B : memref<?xi32> to memref<?xi32>
  return
}


// CHECK-LABEL: func.func @no_copy
// CHECK-NOT: scf.for
func.func @no_copy(%A: memref<4xi32>) {
  %c0 = arith.constant 0 : index
  %0 = memref.load %A[%c0] : memref<4xi32>
  return
}


// CHECK-LABEL: func.func @multi_copy

// CHECK: scf.for %[[I1:[a-zA-Z0-9_]+]]
// CHECK: memref.load %arg0[%[[I1]]]
// CHECK: memref.store

// CHECK: scf.for %[[I2:[a-zA-Z0-9_]+]]
// CHECK: memref.load %arg1[%[[I2]]]
// CHECK: memref.store
func.func @multi_copy(%A: memref<4xi32>, %B: memref<4xi32>, %C: memref<4xi32>) {
  memref.copy %A, %B : memref<4xi32> to memref<4xi32>
  memref.copy %B, %C : memref<4xi32> to memref<4xi32>
  return
}