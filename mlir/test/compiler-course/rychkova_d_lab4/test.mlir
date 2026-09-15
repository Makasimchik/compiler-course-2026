// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir//rychkova_d_lab4_MLIR%shlibext --pass-pipeline="builtin.module(rychkova-copy-to-loop)" %s | FileCheck %s

func.func @test_1d_static(%A: memref<5xf32>, %B: memref<5xf32>) {
  memref.copy %A, %B : memref<5xf32> to memref<5xf32>
  return
}

// CHECK-LABEL: func.func @test_1d_static
// CHECK-DAG: %c0 = arith.constant 0 : index
// CHECK-DAG: %c1 = arith.constant 1 : index
// CHECK-DAG: %c5 = arith.constant 5 : index
// CHECK: scf.for {{.*}} = %c0 to %c5 step %c1 {
// CHECK-NEXT: %{{.*}} = memref.load %arg0[%{{.*}}] : memref<5xf32>
// CHECK-NEXT: memref.store %{{.*}}, %arg1[%{{.*}}] : memref<5xf32>
// CHECK-NEXT: }
// CHECK-NOT: memref.copy

func.func @test_2d_static(%A: memref<3x4xi32>, %B: memref<3x4xi32>) {
  memref.copy %A, %B : memref<3x4xi32> to memref<3x4xi32>
  return
}

// CHECK-LABEL: func.func @test_2d_static
// CHECK-DAG: %c0 = arith.constant 0 : index
// CHECK-DAG: %c1 = arith.constant 1 : index
// CHECK-DAG: %c3 = arith.constant 3 : index
// CHECK-DAG: %c4 = arith.constant 4 : index
// CHECK: scf.for {{.*}} = %c0 to %c3 step %c1 {
// CHECK-NEXT: scf.for {{.*}} = %c0 to %c4 step %c1 {
// CHECK-NEXT: %{{.*}} = memref.load %arg0[%{{.*}}, %{{.*}}] : memref<3x4xi32>
// CHECK-NEXT: memref.store %{{.*}}, %arg1[%{{.*}}, %{{.*}}] : memref<3x4xi32>
// CHECK-NEXT: }
// CHECK-NEXT: }
// CHECK-NOT: memref.copy

func.func @test_3d_static(%A: memref<2x3x2xf64>, %B: memref<2x3x2xf64>) {
  memref.copy %A, %B : memref<2x3x2xf64> to memref<2x3x2xf64>
  return
}

// CHECK-LABEL: func.func @test_3d_static
// CHECK-DAG: %c0 = arith.constant 0 : index
// CHECK-DAG: %c1 = arith.constant 1 : index
// CHECK-DAG: %c2 = arith.constant 2 : index
// CHECK-DAG: %c3 = arith.constant 3 : index
// CHECK: scf.for {{.*}} = %c0 to %c2 step %c1 {
// CHECK-NEXT: scf.for {{.*}} = %c0 to %c3 step %c1 {
// CHECK-NEXT: scf.for {{.*}} = %c0 to %c{{.*}} step %c1 {
// CHECK-NEXT: %{{.*}} = memref.load %arg0[%{{.*}}, %{{.*}}, %{{.*}}] : memref<2x3x2xf64>
// CHECK-NEXT: memref.store %{{.*}}, %arg1[%{{.*}}, %{{.*}}, %{{.*}}] : memref<2x3x2xf64>
// CHECK-NEXT: }
// CHECK-NEXT: }
// CHECK-NEXT: }
// CHECK-NOT: memref.copy

func.func @test_1d_dynamic(%A: memref<?xf32>, %B: memref<?xf32>) {
  memref.copy %A, %B : memref<?xf32> to memref<?xf32>
  return
}

// CHECK-LABEL: func.func @test_1d_dynamic
// CHECK-DAG: %c0 = arith.constant 0 : index
// CHECK-DAG: %c1 = arith.constant 1 : index
// CHECK-DAG: %c{{.*}} = arith.constant 0 : index
// CHECK-DAG: %dim = memref.dim %arg0, %c{{.*}} : memref<?xf32>
// CHECK: scf.for {{.*}} = %c0 to %dim step %c1 {
// CHECK-NEXT: %{{.*}} = memref.load %arg0[%{{.*}}] : memref<?xf32>
// CHECK-NEXT: memref.store %{{.*}}, %arg1[%{{.*}}] : memref<?xf32>
// CHECK-NEXT: }
// CHECK-NOT: memref.copy

func.func @test_2d_dynamic(%A: memref<?x?xi32>, %B: memref<?x?xi32>) {
  memref.copy %A, %B : memref<?x?xi32> to memref<?x?xi32>
  return
}

// CHECK-LABEL: func.func @test_2d_dynamic
// CHECK-DAG: %c0 = arith.constant 0 : index
// CHECK-DAG: %c1 = arith.constant 1 : index
// CHECK-DAG: %c{{.*}} = arith.constant 0 : index
// CHECK-DAG: %dim = memref.dim %arg0, %c{{.*}} : memref<?x?xi32>
// CHECK-DAG: %c{{.*}} = arith.constant 1 : index
// CHECK-DAG: %dim_2 = memref.dim %arg0, %c{{.*}} : memref<?x?xi32>
// CHECK: scf.for {{.*}} = %c0 to %dim step %c1 {
// CHECK-NEXT: scf.for {{.*}} = %c0 to %dim_2 step %c1 {
// CHECK-NEXT: %{{.*}} = memref.load %arg0[%{{.*}}, %{{.*}}] : memref<?x?xi32>
// CHECK-NEXT: memref.store %{{.*}}, %arg1[%{{.*}}, %{{.*}}] : memref<?x?xi32>
// CHECK-NEXT: }
// CHECK-NEXT: }
// CHECK-NOT: memref.copy

func.func @test_multiple_copies(%A: memref<4xi32>, %B: memref<4xi32>, %C: memref<4xi32>) {
  memref.copy %A, %B : memref<4xi32> to memref<4xi32>
  memref.copy %B, %C : memref<4xi32> to memref<4xi32>
  return
}

// CHECK-LABEL: func.func @test_multiple_copies
// CHECK-DAG: %c0 = arith.constant 0 : index
// CHECK-DAG: %c1 = arith.constant 1 : index
// CHECK-DAG: %c4 = arith.constant 4 : index
// CHECK: scf.for {{.*}} = %c0 to %c4 step %c1 {
// CHECK-NEXT: %{{.*}} = memref.load %arg0[%{{.*}}] : memref<4xi32>
// CHECK-NEXT: memref.store %{{.*}}, %arg1[%{{.*}}] : memref<4xi32>
// CHECK-NEXT: }
// CHECK-DAG: %c{{.*}} = arith.constant 0 : index
// CHECK-DAG: %c{{.*}} = arith.constant 1 : index
// CHECK-DAG: %c{{.*}} = arith.constant 4 : index
// CHECK: scf.for {{.*}} = %c{{.*}} to %c{{.*}} step %c{{.*}} {
// CHECK-NEXT: %{{.*}} = memref.load %arg1[%{{.*}}] : memref<4xi32>
// CHECK-NEXT: memref.store %{{.*}}, %arg2[%{{.*}}] : memref<4xi32>
// CHECK-NEXT: }
// CHECK-NOT: memref.copy

func.func @test_without_copy(%A: memref<4xi32>) {
  %c0 = arith.constant 0 : index
  %val = memref.load %A[%c0] : memref<4xi32>
  memref.store %val, %A[%c0] : memref<4xi32>
  return
}

// CHECK-LABEL: func.func @test_without_copy
// CHECK-NOT: scf.for
// CHECK: memref.load
// CHECK: memref.store
