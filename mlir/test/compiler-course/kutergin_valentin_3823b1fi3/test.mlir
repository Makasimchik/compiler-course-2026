// RUN: mlir-opt %s --load-pass-plugin=%mlir_lib_dir/kutergin_valentin_3823b1fi3_MLIR.so -p "builtin.module(lower-memref-copy)" | FileCheck %s

// CHECK-LABEL: func.func @test_unrolling
func.func @test_unrolling(%arg0: memref<10x20xf32>, %arg1: memref<10x20xf32>) {
    // CHECK: scf.for %[[I:.*]] = %c0 to %c10 step %c1
    // CHECK:   scf.for %[[J:.*]] = %c0 to %c20 step %c1

    // CHECK:     %[[VAL:.*]] = memref.load %arg0[%[[I]], %[[J]]]
    // CHECK:     memref.store %[[VAL]], %arg1[%[[I]], %[[J]]]

    // CHECK-NOT: memref.copy

    memref.copy %arg0, %arg1 : memref<10x20xf32> to memref<10x20xf32>
    return
}

// CHECK-LABEL: func.func @copy_1d_int
func.func @copy_1d_int(%arg0: memref<100xi32>, %arg1: memref<100xi32>) {
    // CHECK: scf.for %[[I:.*]] = %c0 to %c100 step %c1
    // CHECK:   %[[VAL:.*]] = memref.load %arg0[%[[I]]] : memref<100xi32>
    // CHECK:   memref.store %[[VAL]], %arg1[%[[I]]] : memref<100xi32>

    // CHECK-NOT: memref.copy

    memref.copy %arg0, %arg1 : memref<100xi32> to memref<100xi32>
    return 
}

// CHECK-LABEL: func.func @copy_3d_float
func.func @copy_3d_float(%arg0: memref<2x4x8xf32>, %arg1: memref<2x4x8xf32>) {
    // CHECK: scf.for %[[I:.*]] = %c0 to %c2 step %c1
    // CHECK:   scf.for %[[J:.*]] = %c0 to %c4 step %c1
    // CHECK:     scf.for %[[K:.*]] = %c0 to %c8 step %c1
    // CHECK:       %[[VAL:.*]] = memref.load %arg0[%[[I]], %[[J]], %[[K]]]
    // CHECK:       memref.store %[[VAL]], %arg1[%[[I]], %[[J]], %[[K]]]

    // CHECK-NOT: memref.copy

    memref.copy %arg0, %arg1 : memref<2x4x8xf32> to memref<2x4x8xf32>
    return
}

// CHECK-LABEL: func.func @copy_tiny
func.func @copy_tiny(%arg0: memref<1x1xf32>, %arg1: memref<1x1xf32>) {
    // CHECK: scf.for %[[I:.*]] = %c0 to %c1 step %c1
    // CHECK:   scf.for %[[J:.*]] = %c0 to %c1 step %c1
    // CHECK:     %[[VAL:.*]] = memref.load %arg0[%[[I]], %[[J]]]
    // CHECK:     memref.store %[[VAL]], %arg1[%[[I]], %[[J]]]

    // CHECK-NOT: memref.copy

    memref.copy %arg0, %arg1 : memref<1x1xf32> to memref<1x1xf32>
    return
}

// CHECK-LABEL: func.func @multiple_copies
func.func @multiple_copies(%arg0: memref<10xf32>, %arg1: memref<10xf32>, %arg2: memref<10xf32>) {
    // CHECK: scf.for %[[IV1:.*]] = %c0 to %c10 step %c1 
    // CHECK:   %[[V1:.*]] = memref.load %arg0[%[[IV1]]]
    // CHECK:   memref.store %[[V1]], %arg1[%[[IV1]]]

    // CHECK-NOT: memref.copy

    // CHECK: scf.for %[[IV2:.*]] = %c0 to %c10 step %c1
    // CHECK:   %[[V2:.*]] = memref.load %arg1[%[[IV2]]]
    // CHECK:   memref.store %[[V2]], %arg2[%[[IV2]]]

    // CHECK-NOT: memref.copy

    memref.copy %arg0, %arg1 : memref<10xf32> to memref<10xf32>
    memref.copy %arg1, %arg2 : memref<10xf32> to memref<10xf32>
    return
}