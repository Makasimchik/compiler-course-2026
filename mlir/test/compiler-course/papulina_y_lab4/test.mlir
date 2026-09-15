// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/papulina_y_lab4_MLIR%shlibext --pass-pipeline="builtin.module(replace-memref-copy)" %s | FileCheck %s

// CHECK-LABEL: func.func @test_1d
func.func @test_1d(%arg0: memref<10xf32>, %arg1: memref<10xf32>) {
  // CHECK-NOT: memref.copy
  // CHECK-DAG: %[[C0:.*]] = arith.constant 0 : index
  // CHECK-DAG: %[[C1:.*]] = arith.constant 1 : index
  // CHECK-DAG: %[[C10:.*]] = arith.constant 10 : index
  // CHECK:      scf.for %[[I:.*]] = %[[C0]] to %[[C10]]
  // CHECK-NEXT:   %[[V:.*]] = memref.load %arg0[%[[I]]]
  // CHECK-NEXT:   memref.store %[[V]], %arg1[%[[I]]]
  memref.copy %arg0, %arg1 : memref<10xf32> to memref<10xf32>
  return
}

// CHECK-LABEL: func.func @test_2d
func.func @test_2d(%arg0: memref<4x8xf32>, %arg1: memref<4x8xf32>) {
  // CHECK:      scf.for %[[I:.*]] = {{.*}} to %c4
  // CHECK-NEXT:   scf.for %[[J:.*]] = {{.*}} to %c8
  // CHECK-NEXT:     %[[V:.*]] = memref.load %arg0[%[[I]], %[[J]]]
  // CHECK-NEXT:     memref.store %[[V]], %arg1[%[[I]], %[[J]]]
  memref.copy %arg0, %arg1 : memref<4x8xf32> to memref<4x8xf32>
  return
}

// CHECK-LABEL: func.func @test_several_copies
func.func @test_several_copies(%arg0: memref<10xf32>, %arg1: memref<10xf32>, %arg2: memref<10xf32>) {
  // CHECK:      scf.for %[[I1:.*]] = {{.*}}
  // CHECK-NEXT:   %[[V1:.*]] = memref.load %arg0[%[[I1]]]
  // CHECK-NEXT:   memref.store %[[V1]], %arg1[%[[I1]]]
  
  // CHECK:      scf.for %[[I2:.*]] = {{.*}}
  // CHECK-NEXT:   %[[V2:.*]] = memref.load %arg1[%[[I2]]]
  // CHECK-NEXT:   memref.store %[[V2]], %arg2[%[[I2]]]
  
  memref.copy %arg0, %arg1 : memref<10xf32> to memref<10xf32>
  memref.copy %arg1, %arg2 : memref<10xf32> to memref<10xf32>
  return
}

// CHECK-LABEL: func.func @test_3d_copy
func.func @test_3d_copy(%arg0: memref<2x3x4xi32>, %arg1: memref<2x3x4xi32>) {
  // CHECK:      scf.for %[[I:.*]] = {{.*}} to %c2
  // CHECK-NEXT:   scf.for %[[J:.*]] = {{.*}} to %c3
  // CHECK-NEXT:     scf.for %[[K:.*]] = {{.*}} to %c4
  // CHECK-NEXT:       %[[VAL:.*]] = memref.load %arg0[%[[I]], %[[J]], %[[K]]]
  // CHECK-NEXT:       memref.store %[[VAL]], %arg1[%[[I]], %[[J]], %[[K]]]
  memref.copy %arg0, %arg1 : memref<2x3x4xi32> to memref<2x3x4xi32>
  return
}