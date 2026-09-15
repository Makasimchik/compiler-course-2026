// RUN: mlir-opt %s --load-pass-plugin=%mlir_lib_dir/ReplaceMemref_chacshin_v_3823b1fi3_MLIR.so -p "builtin.module(func.func(expand-memcopy))" | FileCheck %s

// CHECK-LABEL: func.func @copy_dynamic_2d
// CHECK-SAME: (%[[SRC:.*]]: memref<?x?xf32>, %[[DST:.*]]: memref<?x?xf32>)
func.func @copy_dynamic_2d(%src: memref<?x?xf32>, %dst: memref<?x?xf32>) {
    // CHECK-DAG: %[[C0:.*]] = arith.constant 0 : index
    // CHECK-DAG: %[[C1:.*]] = arith.constant 1 : index
    // CHECK: %[[D0:.*]] = memref.dim %[[SRC]], %[[C0]]
    // CHECK: %[[D1:.*]] = memref.dim %[[SRC]], %[[C1]]
    // CHECK: scf.for %[[I:.*]] = %[[C0]] to %[[D0]] step %[[C1]]
    // CHECK:   scf.for %[[J:.*]] = %[[C0]] to %[[D1]] step %[[C1]]
    // CHECK:     %[[VAL:.*]] = memref.load %[[SRC]][%[[I]], %[[J]]]
    // CHECK:     memref.store %[[VAL]], %[[DST]][%[[I]], %[[J]]]
    
    memref.copy %src, %dst : memref<?x?xf32> to memref<?x?xf32>
    return
}

// CHECK-LABEL: func.func @copy_scalar
// CHECK-SAME: (%[[SRC:.*]]: memref<f32>, %[[DST:.*]]: memref<f32>)
func.func @copy_scalar(%src: memref<f32>, %dst: memref<f32>) {
    // CHECK-NOT: scf.for
    // CHECK: %[[ELEM:.*]] = memref.load %[[SRC]][]
    // CHECK: memref.store %[[ELEM]], %[[DST]][]
    // CHECK-NOT: memref.copy
    
    memref.copy %src, %dst : memref<f32> to memref<f32>
    return
}

// CHECK-LABEL: func.func @copy_mixed_ranked
func.func @copy_mixed_ranked(%arg0: memref<16x?xi32>, %arg1: memref<16x?xi32>) {
    // CHECK: %[[C16:.*]] = arith.constant 16 : index
    // CHECK: %[[D1:.*]] = memref.dim %arg0, %c1
    // CHECK: scf.for %{{.*}} = %c0 to %[[C16]]
    // CHECK:   scf.for %{{.*}} = %c0 to %[[D1]]
    
    memref.copy %arg0, %arg1 : memref<16x?xi32> to memref<16x?xi32>
    return
}

// CHECK-LABEL: func.func @copy_vector_data
func.func @copy_vector_data(%arg0: memref<5xvector<4xf32>>, %arg1: memref<5xvector<4xf32>>) {
    // CHECK: scf.for %[[IV:.*]] = %c0 to %c5 step %c1
    // CHECK:   %[[V:.*]] = memref.load %arg0[%[[IV]]] : memref<5xvector<4xf32>>
    // CHECK:   memref.store %[[V]], %arg1[%[[IV]]] : memref<5xvector<4xf32>>
    
    memref.copy %arg0, %arg1 : memref<5xvector<4xf32>> to memref<5xvector<4xf32>>
    return
}

// CHECK-LABEL: func.func @copy_inside_region
func.func @copy_inside_region(%arg0: memref<10xf32>, %arg1: memref<10xf32>, %lb: index, %ub: index, %step: index) {
    // CHECK: scf.for
    // CHECK:   scf.for %[[I:.*]] = %c0 to %c10 step %c1
    // CHECK:     memref.load
    // CHECK:     memref.store
    scf.for %i = %lb to %ub step %step {
        memref.copy %arg0, %arg1 : memref<10xf32> to memref<10xf32>
    }
    return
}