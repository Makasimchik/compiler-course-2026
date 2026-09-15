// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/smyshlaev_a_lab4_MLIR%shlibext --pass-pipeline="builtin.module(func.func(lower-memref-copy-to-scf))" %s | FileCheck %s


// CHECK-LABEL: func.func @copy_1d_int
func.func @copy_1d_int(%arg0: memref<100xi32>, %arg1: memref<100xi32>) {
    // CHECK-NOT: memref.copy
    // CHECK: %[[DIM:.*]] = memref.dim %arg0, %c0
    // CHECK: scf.for %[[I:.*]] = %{{.*}} to %[[DIM]] step %{{.*}} {
    // CHECK-NEXT:   %[[VAL:.*]] = memref.load %arg0[%[[I]]]
    // CHECK-NEXT:   memref.store %[[VAL]], %arg1[%[[I]]]
    // CHECK-NEXT: }
    memref.copy %arg0, %arg1 : memref<100xi32> to memref<100xi32>
    return 
}

// CHECK-LABEL: func.func @test_2d
func.func @test_2d(%arg0: memref<10x20xf32>, %arg1: memref<10x20xf32>) {
    // CHECK-NOT: memref.copy
    // CHECK: %[[DIM0:.*]] = memref.dim %arg0, %c0
    // CHECK: %[[DIM1:.*]] = memref.dim %arg0, %c1
    // CHECK: scf.for %[[I:.*]] = %{{.*}} to %[[DIM0]] step %{{.*}} {
    // CHECK-NEXT:   scf.for %[[J:.*]] = %{{.*}} to %[[DIM1]] step %{{.*}} {
    // CHECK-NEXT:     %[[VAL:.*]] = memref.load %arg0[%[[I]], %[[J]]]
    // CHECK-NEXT:     memref.store %[[VAL]], %arg1[%[[I]], %[[J]]]
    // CHECK-NEXT:   }
    // CHECK-NEXT: }
    memref.copy %arg0, %arg1 : memref<10x20xf32> to memref<10x20xf32>
    return
}

// CHECK-LABEL: func.func @copy_3d_float
func.func @copy_3d_float(%arg0: memref<2x4x8xf32>, %arg1: memref<2x4x8xf32>) {
    // CHECK-NOT: memref.copy
    // CHECK: %[[D0:.*]] = memref.dim %arg0, %c0
    // CHECK: %[[D1:.*]] = memref.dim %arg0, %c1
    // CHECK: %[[D2:.*]] = memref.dim %arg0, %c2
    // CHECK: scf.for %[[I:.*]] = %{{.*}} to %[[D0]] step %{{.*}} {
    // CHECK-NEXT:   scf.for %[[J:.*]] = %{{.*}} to %[[D1]] step %{{.*}} {
    // CHECK-NEXT:     scf.for %[[K:.*]] = %{{.*}} to %[[D2]] step %{{.*}} {
    // CHECK-NEXT:       %[[VAL:.*]] = memref.load %arg0[%[[I]], %[[J]], %[[K]]]
    // CHECK-NEXT:       memref.store %[[VAL]], %arg1[%[[I]], %[[J]], %[[K]]]
    // CHECK-NEXT:     }
    // CHECK-NEXT:   }
    // CHECK-NEXT: }
    memref.copy %arg0, %arg1 : memref<2x4x8xf32> to memref<2x4x8xf32>
    return
}

// CHECK-LABEL: func.func @copy_tiny
func.func @copy_tiny(%arg0: memref<1x1xf32>, %arg1: memref<1x1xf32>) {
    // CHECK-NOT: memref.copy
    // CHECK: %[[D0:.*]] = memref.dim %arg0, %c0
    // CHECK: %[[D1:.*]] = memref.dim %arg0, %c1
    // CHECK: scf.for %[[I:.*]] = %{{.*}} to %[[D0]] step %{{.*}} {
    // CHECK:   scf.for %[[J:.*]] = %{{.*}} to %[[D1]] step %{{.*}} {
    // CHECK:     %[[V:.*]] = memref.load %arg0[%[[I]], %[[J]]]
    // CHECK:     memref.store %[[V]], %arg1[%[[I]], %[[J]]]
    memref.copy %arg0, %arg1 : memref<1x1xf32> to memref<1x1xf32>
    return
}

// CHECK-LABEL: func.func @multiple_copies
func.func @multiple_copies(%arg0: memref<10xf32>, %arg1: memref<10xf32>, %arg2: memref<10xf32>) {
    // CHECK-NOT: memref.copy
    // CHECK: %[[DIM1:.*]] = memref.dim %arg0, %c0
    // CHECK: scf.for %[[IV1:.*]] = %{{.*}} to %[[DIM1]] step %{{.*}} {
    // CHECK:   %[[V1:.*]] = memref.load %arg0[%[[IV1]]]
    // CHECK:   memref.store %[[V1]], %arg1[%[[IV1]]]
    // CHECK: }
    
    // CHECK: %[[DIM2:.*]] = memref.dim %arg1, %c0
    // CHECK: scf.for %[[IV2:.*]] = %{{.*}} to %[[DIM2]] step %{{.*}} {
    // CHECK:   %[[V2:.*]] = memref.load %arg1[%[[IV2]]]
    // CHECK:   memref.store %[[V2]], %arg2[%[[IV2]]]
    // CHECK: }
    memref.copy %arg0, %arg1 : memref<10xf32> to memref<10xf32>
    memref.copy %arg1, %arg2 : memref<10xf32> to memref<10xf32>
    return
}