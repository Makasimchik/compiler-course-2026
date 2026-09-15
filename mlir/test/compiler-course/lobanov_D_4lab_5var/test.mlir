// RUN: mlir-opt \
// RUN: -load-pass-plugin=%mlir_lib_dir/lobanov_D_4lab_5var_MLIR%shlibext \
// RUN: --pass-pipeline="builtin.module(expand-memref-copy-to-scf)" \
// RUN: %s | FileCheck %s

// 1D с плавающей точкой

func.func @copy_float_vector(%src: memref<8xf64>, %dst: memref<8xf64>) {
  memref.copy %src, %dst : memref<8xf64> to memref<8xf64>
  return
}

// CHECK-LABEL: func.func @copy_float_vector
// CHECK: %[[C0:.*]] = arith.constant 0 : index
// CHECK: %[[C1:.*]] = arith.constant 1 : index
// CHECK: %[[C8:.*]] = arith.constant 8 : index
// CHECK: scf.for %[[IV:.*]] = %[[C0]] to %[[C8]] step %[[C1]]
// CHECK: memref.load %{{.*}}[%[[IV]]]
// CHECK: memref.store %{{.*}}, %{{.*}}[%[[IV]]]
// CHECK-NOT: memref.copy

// 3D статический массив

func.func @copy_3d_tensor(%src: memref<2x3x4xi16>, %dst: memref<2x3x4xi16>) {
  memref.copy %src, %dst : memref<2x3x4xi16> to memref<2x3x4xi16>
  return
}

// CHECK-LABEL: func.func @copy_3d_tensor
// CHECK: %[[C0:.*]] = arith.constant 0 : index
// CHECK: %[[C1:.*]] = arith.constant 1 : index
// CHECK: %[[C2:.*]] = arith.constant 2 : index
// CHECK: %[[C3:.*]] = arith.constant 3 : index
// CHECK: %[[C4:.*]] = arith.constant 4 : index
// CHECK: scf.for %{{.*}} = %[[C0]] to %[[C2]] step %[[C1]]
// CHECK:   scf.for %{{.*}} = %[[C0]] to %[[C3]] step %[[C1]]
// CHECK:     scf.for %{{.*}} = %[[C0]] to %[[C4]] step %[[C1]]
// CHECK:       memref.load
// CHECK:       memref.store
// CHECK-NOT: memref.copy

// Полностью динамический массив

func.func @copy_fully_dynamic(%src: memref<?x?xf32>, %dst: memref<?x?xf32>) {
  memref.copy %src, %dst : memref<?x?xf32> to memref<?x?xf32>
  return
}

// CHECK-LABEL: func.func @copy_fully_dynamic
// CHECK: %[[C0:.*]] = arith.constant 0 : index
// CHECK: %[[C1:.*]] = arith.constant 1 : index
// CHECK: %[[DIM0:.*]] = memref.dim %{{.*}}, %[[C0]]
// CHECK: %[[DIM1:.*]] = memref.dim %{{.*}}, %[[C1]]
// CHECK: scf.for %{{.*}} = %[[C0]] to %[[DIM0]] step %[[C1]]
// CHECK:   scf.for %{{.*}} = %[[C0]] to %[[DIM1]] step %[[C1]]
// CHECK:     memref.load
// CHECK:     memref.store
// CHECK-NOT: memref.copy

// Копирование с нулевым измерением (edge case)

func.func @copy_zero_dim(%src: memref<0xi32>, %dst: memref<0xi32>) {
  memref.copy %src, %dst : memref<0xi32> to memref<0xi32>
  return
}

// CHECK-LABEL: func.func @copy_zero_dim
// CHECK: %[[C0:.*]] = arith.constant 0 : index
// CHECK: %[[C1:.*]] = arith.constant 1 : index
// CHECK: scf.for %{{.*}} = %[[C0]] to %[[C0]] step %[[C1]]
// CHECK-NOT: memref.copy

// Три копирования подряд

func.func @triple_copy(%a: memref<5xi32>, %b: memref<5xi32>, %c: memref<5xi32>, %d: memref<5xi32>) {
  memref.copy %a, %b : memref<5xi32> to memref<5xi32>
  memref.copy %b, %c : memref<5xi32> to memref<5xi32>
  memref.copy %c, %d : memref<5xi32> to memref<5xi32>
  return
}

// CHECK-LABEL: func.func @triple_copy
// CHECK: scf.for
// CHECK: scf.for
// CHECK: scf.for
// CHECK: memref.load
// CHECK: memref.store
// CHECK-NOT: memref.copy

// Функция без копирования

func.func @arithmetic_only(%a: memref<4xi32>, %b: memref<4xi32>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %x = memref.load %a[%c0] : memref<4xi32>
  %y = memref.load %b[%c1] : memref<4xi32>
  %sum = arith.addi %x, %y : i32
  memref.store %sum, %a[%c1] : memref<4xi32>
  return
}

// CHECK-LABEL: func.func @arithmetic_only
// CHECK-NOT: scf.for
// CHECK-NOT: memref.copy
// CHECK: arith.addi

// самокопирование

func.func @self_copy(%src: memref<16xi64>) {
  memref.copy %src, %src : memref<16xi64> to memref<16xi64>
  return
}

// CHECK-LABEL: func.func @self_copy
// CHECK: scf.for
// CHECK: memref.load
// CHECK: memref.store
// CHECK-NOT: memref.copy

// 1D с index

func.func @copy_index_type(%src: memref<3xindex>, %dst: memref<3xindex>) {
  memref.copy %src, %dst : memref<3xindex> to memref<3xindex>
  return
}

// CHECK-LABEL: func.func @copy_index_type
// CHECK: scf.for
// CHECK: memref.load
// CHECK: memref.store
// CHECK-NOT: memref.copy

// Вложенные вызовы функций с копированием

func.func @outer_function(%a: memref<4xi32>, %b: memref<4xi32>) {
  memref.copy %a, %b : memref<4xi32> to memref<4xi32>
  return
}

func.func @inner_function(%x: memref<4xi32>, %y: memref<4xi32>) {
  memref.copy %x, %y : memref<4xi32> to memref<4xi32>
  call @outer_function(%x, %y) : (memref<4xi32>, memref<4xi32>) -> ()
  return
}

// CHECK-LABEL: func.func @outer_function
// CHECK: scf.for
// CHECK: memref.load
// CHECK: memref.store

// CHECK-LABEL: func.func @inner_function
// CHECK: scf.for
// CHECK: memref.load
// CHECK: memref.store
// CHECK: call @outer_function
// CHECK-NOT: memref.copy