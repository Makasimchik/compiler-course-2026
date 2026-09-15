// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/gutyansky_a_lab4_MLIR%shlibext --pass-pipeline="builtin.module(gutyansky_a_lab4_MLIR)" %s | FileCheck %s

// CHECK-LABEL: func.func @copy_1d_static
func.func @copy_1d_static(%src: memref<8xf32>, %dst: memref<8xf32>) {
  // CHECK-NOT: memref.copy
  // CHECK-DAG: %[[C0:.*]]  = arith.constant 0 : index
  // CHECK-DAG: %[[C1:.*]]  = arith.constant 1 : index
  // CHECK-DAG: %[[C8:.*]]  = arith.constant 8 : index
  // CHECK:     scf.for %[[IV:.*]] = %[[C0]] to %[[C8]] step %[[C1]] {
  // CHECK:       %[[VAL:.*]] = memref.load %{{.*}}[%[[IV]]]
  // CHECK:       memref.store %[[VAL]], %{{.*}}[%[[IV]]]
  // CHECK:     }
  memref.copy %src, %dst : memref<8xf32> to memref<8xf32>
  return
}

// CHECK-LABEL: func.func @copy_3d_static
func.func @copy_3d_static(%src: memref<2x3x4xi8>, %dst: memref<2x3x4xi8>) {
  // CHECK-NOT: memref.copy
  // CHECK:     scf.for %{{.*}} = %{{.*}} to %{{.*}} step %{{.*}} {
  // CHECK:       scf.for %{{.*}} = %{{.*}} to %{{.*}} step %{{.*}} {
  // CHECK:         scf.for %{{.*}} = %{{.*}} to %{{.*}} step %{{.*}} {
  // CHECK:           memref.load
  // CHECK:           memref.store
  // CHECK:         }
  // CHECK:       }
  // CHECK:     }
  memref.copy %src, %dst : memref<2x3x4xi8> to memref<2x3x4xi8>
  return
}

// CHECK-LABEL: func.func @copy_1d_dynamic
func.func @copy_1d_dynamic(%src: memref<?xf64>, %dst: memref<?xf64>) {
  // CHECK-NOT: memref.copy
  // CHECK-DAG: %[[C0:.*]]  = arith.constant 0 : index
  // CHECK-DAG: %[[C1:.*]]  = arith.constant 1 : index
  // CHECK-DAG: %[[C0_0:.*]] = arith.constant 0 : index
  // CHECK:     %[[DIM:.*]] = memref.dim %{{.*}}, %[[C0_0]] : memref<?xf64>
  // CHECK:     scf.for %{{.*}} = %[[C0]] to %[[DIM]] step %[[C1]] {
  // CHECK:       memref.load
  // CHECK:       memref.store
  // CHECK:     }
  memref.copy %src, %dst : memref<?xf64> to memref<?xf64>
  return
}

// CHECK-LABEL: func.func @copy_2d_mixed
func.func @copy_2d_mixed(%src: memref<?x16xf32>, %dst: memref<?x16xf32>) {
  // CHECK-NOT: memref.copy
  // CHECK-DAG: %[[C0:.*]]   = arith.constant 0 : index
  // CHECK-DAG: %[[C1:.*]]   = arith.constant 1 : index
  // CHECK-DAG: %[[C0_0:.*]] = arith.constant 0 : index
  // CHECK:     %[[DIM0:.*]] = memref.dim %{{.*}}, %[[C0_0]] : memref<?x16xf32>
  // CHECK:     %[[C16:.*]]  = arith.constant 16 : index
  // CHECK:     scf.for %{{.*}} = %[[C0]] to %[[DIM0]] step %[[C1]] {
  // CHECK:       scf.for %{{.*}} = %[[C0]] to %[[C16]] step %[[C1]] {
  // CHECK:         memref.load
  // CHECK:         memref.store
  // CHECK:       }
  // CHECK:     }
  memref.copy %src, %dst : memref<?x16xf32> to memref<?x16xf32>
  return
}

// CHECK-LABEL: func.func @copy_size_one_dim
func.func @copy_size_one_dim(%src: memref<1x4xf32>, %dst: memref<1x4xf32>) {
  // CHECK-NOT: memref.copy
  // CHECK-DAG: %[[C0:.*]]   = arith.constant 0 : index
  // CHECK-DAG: %[[C1:.*]]   = arith.constant 1 : index
  // CHECK-DAG: %[[C1_0:.*]] = arith.constant 1 : index
  // CHECK-DAG: %[[C4:.*]]   = arith.constant 4 : index
  // CHECK:     scf.for %{{.*}} = %[[C0]] to %[[C1_0]] step %[[C1]] {
  // CHECK:       scf.for %{{.*}} = %[[C0]] to %[[C4]] step %[[C1]] {
  // CHECK:         memref.load
  // CHECK:         memref.store
  // CHECK:       }
  // CHECK:     }
  memref.copy %src, %dst : memref<1x4xf32> to memref<1x4xf32>
  return
}

// CHECK-LABEL: func.func @copy_inside_loop
func.func @copy_inside_loop(%src: memref<4xf32>, %dst: memref<4xf32>, %n: index) {
  // CHECK-NOT: memref.copy
  // CHECK:     scf.for %{{.*}} = %{{.*}} to %{{.*}} step %{{.*}} {
    
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  scf.for %i = %c0 to %n step %c1 {
    // CHECK:     scf.for %{{.*}} = %{{.*}} to %{{.*}} step %{{.*}} {
    // CHECK:       memref.load
    // CHECK:       memref.store
    // CHECK:     }
    
    memref.copy %src, %dst : memref<4xf32> to memref<4xf32>
  }
  return
}