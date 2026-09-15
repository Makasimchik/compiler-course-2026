// RUN: mlir-opt %s --load-pass-plugin=%mlir_lib_dir/nikitina_v_lab4_MLIR.so --pass-pipeline="builtin.module(func.func(memref-copy-to-loops))" | FileCheck %s

// CHECK-LABEL: func @test_1d_copy
func.func @test_1d_copy(%arg0: memref<100xf32>, %arg1: memref<100xf32>) {
  // CHECK-NOT: memref.copy
  // CHECK: scf.for %[[IV:.*]] =
  // CHECK:   %[[VAL:.*]] = memref.load %arg0[%[[IV]]]
  // CHECK:   memref.store %[[VAL]], %arg1[%[[IV]]]
  memref.copy %arg0, %arg1 : memref<100xf32> to memref<100xf32>
  return
}

// CHECK-LABEL: func @test_2d_copy
func.func @test_2d_copy(%arg0: memref<10x20xi32>, %arg1: memref<10x20xi32>) {
  // CHECK-NOT: memref.copy
  // CHECK: scf.for %[[IV0:.*]] =
  // CHECK:   scf.for %[[IV1:.*]] =
  // CHECK:     %[[VAL:.*]] = memref.load %arg0[%[[IV0]], %[[IV1]]]
  // CHECK:     memref.store %[[VAL]], %arg1[%[[IV0]], %[[IV1]]]
  memref.copy %arg0, %arg1 : memref<10x20xi32> to memref<10x20xi32>
  return
}

// CHECK-LABEL: func @test_dynamic_copy
func.func @test_dynamic_copy(%arg0: memref<?x?xf64>, %arg1: memref<?x?xf64>) {
  // CHECK-NOT: memref.copy
  // CHECK: %[[DIM0:.*]] = memref.dim %arg0
  // CHECK: %[[DIM1:.*]] = memref.dim %arg0
  // CHECK: scf.for %[[IV0:.*]] = {{.*}} to %[[DIM0]]
  // CHECK:   scf.for %[[IV1:.*]] = {{.*}} to %[[DIM1]]
  // CHECK:     %[[VAL:.*]] = memref.load %arg0[%[[IV0]], %[[IV1]]]
  // CHECK:     memref.store %[[VAL]], %arg1[%[[IV0]], %[[IV1]]]
  memref.copy %arg0, %arg1 : memref<?x?xf64> to memref<?x?xf64>
  return
}