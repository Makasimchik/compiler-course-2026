// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/shvetsova_copy_with_load_store_MLIR%shlibext --pass-pipeline="builtin.module(lower-memref-copy)" %s | FileCheck %s

module {

  // CHECK-LABEL: func.func @test_scalar_copy
  func.func @test_scalar_copy(%src: memref<f32>, %dst: memref<f32>) {
    // CHECK-NEXT: %[[VAL:.*]] = memref.load %arg0[] : memref<f32>
    // CHECK-NEXT: memref.store %[[VAL]], %arg1[] : memref<f32>
    // CHECK-NOT: memref.copy
    memref.copy %src, %dst : memref<f32> to memref<f32>
    return
  }

  // CHECK-LABEL: func.func @test_mixed_copy
  func.func @test_mixed_copy(%src: memref<10x?xf32>, %dst: memref<10x?xf32>) {
    // CHECK-DAG: %[[C10:.*]] = arith.constant 10 : index
    // CHECK-DAG: %[[C1:.*]] = arith.constant 1 : index
    // CHECK-DAG: %[[D1:.*]] = memref.dim %arg0, %[[C1]] : memref<10x?xf32>
    
    // CHECK: scf.for %[[I:.*]] = {{.*}} to %[[C10]]
    // CHECK:   scf.for %[[J:.*]] = {{.*}} to %[[D1]]
    memref.copy %src, %dst : memref<10x?xf32> to memref<10x?xf32>
    return
  }

  // CHECK-LABEL: func.func @test_3d_copy
  func.func @test_3d_copy(%src: memref<2x3x4xi32>, %dst: memref<2x3x4xi32>) {
    // CHECK: scf.for %arg2
    // CHECK:   scf.for %arg3
    // CHECK:     scf.for %arg4
    // CHECK:       %[[V:.*]] = memref.load %arg0[%arg2, %arg3, %arg4]
    // CHECK:       memref.store %[[V]], %arg1[%arg2, %arg3, %arg4]
    memref.copy %src, %dst : memref<2x3x4xi32> to memref<2x3x4xi32>
    return
  }

  // CHECK-LABEL: func.func @test_i64_copy
  func.func @test_i64_copy(%src: memref<?xi64>, %dst: memref<?xi64>) {
    // CHECK: %[[VAL:.*]] = memref.load %arg0[%{{.*}}] : memref<?xi64>
    // CHECK: memref.store %[[VAL]], %arg1[%{{.*}}] : memref<?xi64>
    memref.copy %src, %dst : memref<?xi64> to memref<?xi64>
    return
  }

  // CHECK-LABEL: func.func @test_empty_copy
  func.func @test_empty_copy(%src: memref<0xf32>, %dst: memref<0xf32>) {
    // CHECK: %[[C0:.*]] = arith.constant 0 : index
    // CHECK: scf.for %{{.*}} = %{{.*}} to %[[C0]]
    memref.copy %src, %dst : memref<0xf32> to memref<0xf32>
    return
  }

  // CHECK-LABEL: func.func @test_dynamic_2d
  func.func @test_dynamic_2d(%src: memref<?x?xf32>, %dst: memref<?x?xf32>) {
  // CHECK: %[[D0:.*]] = memref.dim %arg0, %{{.*}} : memref<?x?xf32>
  // CHECK: %[[D1:.*]] = memref.dim %arg0, %{{.*}} : memref<?x?xf32>
  // CHECK: scf.for %[[I:.*]] = {{.*}} to %[[D0]]
  // CHECK:   scf.for %[[J:.*]] = {{.*}} to %[[D1]]
  // CHECK:     %[[V:.*]] = memref.load %arg0[%[[I]], %[[J]]] : memref<?x?xf32>
  // CHECK:     memref.store %[[V]], %arg1[%[[I]], %[[J]]] : memref<?x?xf32>
  memref.copy %src, %dst : memref<?x?xf32> to memref<?x?xf32>
  return
}

// CHECK-LABEL: func.func @test_1d_static
  func.func @test_1d_static(%src: memref<8xi32>, %dst: memref<8xi32>) {
  // CHECK: %[[C8:.*]] = arith.constant 8 : index
  // CHECK: scf.for %[[I:.*]] = {{.*}} to %[[C8]]
  // CHECK:   %[[V:.*]] = memref.load %arg0[%[[I]]] : memref<8xi32>
  // CHECK:   memref.store %[[V]], %arg1[%[[I]]] : memref<8xi32>
  memref.copy %src, %dst : memref<8xi32> to memref<8xi32>
  return
}
}