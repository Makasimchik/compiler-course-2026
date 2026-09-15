// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/ZeninALab4_MLIR%shlibext --pass-pipeline="builtin.module(zenin-memref-copy)" %s | FileCheck %s


// CHECK-LABEL: func.func @copy_test
// CHECK: %[[C0:.*]] = arith.constant 0 : index
// CHECK-NEXT: %[[C1:.*]] = arith.constant 1 : index
// CHECK-NEXT: %[[C4:.*]] = arith.constant 4 : index
// CHECK-NEXT: scf.for %[[I:.*]] = %[[C0]] to %[[C4]] step %[[C1]] {
// CHECK-NEXT:   %[[VAL:.*]] = memref.load
// CHECK-NEXT:   memref.store
// CHECK-NEXT: }
// CHECK-NOT: memref.copy
func.func @copy_test(%src: memref<4xi32>, %dst: memref<4xi32>) {
  memref.copy %src, %dst : memref<4xi32> to memref<4xi32>
  return
}


// CHECK-LABEL: func.func @copy_size8
// CHECK: %[[C8:.*]] = arith.constant 8 : index
// CHECK: scf.for {{.*}} to %[[C8]]
// CHECK-NOT: memref.copy
func.func @copy_size8(%src: memref<8xi32>, %dst: memref<8xi32>) {
  memref.copy %src, %dst : memref<8xi32> to memref<8xi32>
  return
}

// CHECK-LABEL: func.func @copy_f32
// CHECK: scf.for
// CHECK: memref.load
// CHECK-NEXT: memref.store
// CHECK-NOT: memref.copy
func.func @copy_f32(%src: memref<4xf32>, %dst: memref<4xf32>) {
  memref.copy %src, %dst : memref<4xf32> to memref<4xf32>
  return
}

// CHECK-LABEL: func.func @copy_dynamic
// CHECK: memref.copy
// CHECK-NOT: scf.for
func.func @copy_dynamic(%src: memref<?xi32>, %dst: memref<?xi32>) {
  memref.copy %src, %dst : memref<?xi32> to memref<?xi32>
  return
}

// CHECK-LABEL: func.func @copy_2d
// CHECK: memref.copy
// CHECK-NOT: scf.for
func.func @copy_2d(%src: memref<4x4xi32>, %dst: memref<4x4xi32>) {
  memref.copy %src, %dst : memref<4x4xi32> to memref<4x4xi32>
  return
}