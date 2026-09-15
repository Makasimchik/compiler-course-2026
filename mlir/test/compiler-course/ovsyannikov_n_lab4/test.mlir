// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/ovsyannikov_n_lab4_MLIR%shlibext --pass-pipeline="builtin.module(ovsyannikov-loop-fusion)" %s | FileCheck %s

func.func @test_success(%arg0: memref<10xf32>, %arg1: memref<10xf32>, %arg2: memref<10xf32>) {
  %c0 = arith.constant 0 : index
  %c10 = arith.constant 10 : index
  %c1 = arith.constant 1 : index
  // CHECK: scf.for %[[I:.*]] = %c0 to %c10 step %c1
  // CHECK-NEXT: %[[V1:.*]] = memref.load %arg0[%[[I]]]
  // CHECK-NEXT: memref.store %[[V1]], %arg1[%[[I]]]
  // CHECK-NEXT: %[[V2:.*]] = memref.load %arg1[%[[I]]]
  // CHECK-NEXT: memref.store %[[V2]], %arg2[%[[I]]]
  scf.for %i = %c0 to %c10 step %c1 {
    %v = memref.load %arg0[%i] : memref<10xf32>
    memref.store %v, %arg1[%i] : memref<10xf32>
  }
  scf.for %j = %c0 to %c10 step %c1 {
    %v2 = memref.load %arg1[%j] : memref<10xf32>
    memref.store %v2, %arg2[%j] : memref<10xf32>
  }
  return
}

func.func @test_triple_fusion(%arg0: memref<10xf32>) {
  %c0 = arith.constant 0 : index
  %c10 = arith.constant 10 : index
  %c1 = arith.constant 1 : index
  %f1 = arith.constant 1.0 : f32
  // CHECK: scf.for
  // CHECK-NOT: scf.for
  scf.for %i = %c0 to %c10 step %c1 {
    memref.store %f1, %arg0[%i] : memref<10xf32>
  }
  scf.for %j = %c0 to %c10 step %c1 {
    memref.store %f1, %arg0[%j] : memref<10xf32>
  }
  scf.for %k = %c0 to %c10 step %c1 {
    memref.store %f1, %arg0[%k] : memref<10xf32>
  }
  return
}

func.func @test_fail_bounds(%arg0: memref<10xf32>) {
  %c0 = arith.constant 0 : index
  %c5 = arith.constant 5 : index
  %c10 = arith.constant 10 : index
  %c1 = arith.constant 1 : index
  %f1 = arith.constant 1.0 : f32
  // CHECK: scf.for
  // CHECK: scf.for
  scf.for %i = %c0 to %c5 step %c1 {
    memref.store %f1, %arg0[%i] : memref<10xf32>
  }
  scf.for %j = %c0 to %c10 step %c1 {
    memref.store %f1, %arg0[%j] : memref<10xf32>
  }
  return
}

func.func @test_fail_step(%arg0: memref<10xf32>) {
  %c0 = arith.constant 0 : index
  %c10 = arith.constant 10 : index
  %c1 = arith.constant 1 : index
  %c2 = arith.constant 2 : index
  %f1 = arith.constant 1.0 : f32
  // CHECK: scf.for
  // CHECK: scf.for
  scf.for %i = %c0 to %c10 step %c1 {
    memref.store %f1, %arg0[%i] : memref<10xf32>
  }
  scf.for %j = %c0 to %c10 step %c2 {
    memref.store %f1, %arg0[%j] : memref<10xf32>
  }
  return
}

func.func @test_dynamic_success(%arg0: index, %arg1: index, %arg2: index, %arg3: memref<?xf32>) {
  %f1 = arith.constant 1.0 : f32
  // CHECK: scf.for %[[IDX:.*]] = %arg0 to %arg1 step %arg2
  // CHECK-NEXT: memref.store %{{.*}}, %arg3[%[[IDX]]]
  // CHECK-NEXT: memref.store %{{.*}}, %arg3[%[[IDX]]]
  scf.for %i = %arg0 to %arg1 step %arg2 {
    memref.store %f1, %arg3[%i] : memref<?xf32>
  }
  scf.for %j = %arg0 to %arg1 step %arg2 {
    memref.store %f1, %arg3[%j] : memref<?xf32>
  }
  return
}

func.func @test_dynamic_fail(%arg0: index, %arg1: index, %arg2: index, %arg3: index, %arg4: memref<?xf32>) {
  %f1 = arith.constant 1.0 : f32
  // CHECK: scf.for
  // CHECK: scf.for
  scf.for %i = %arg0 to %arg1 step %arg2 {
    memref.store %f1, %arg4[%i] : memref<?xf32>
  }
  scf.for %j = %arg0 to %arg3 step %arg2 {
    memref.store %f1, %arg4[%j] : memref<?xf32>
  }
  return
}
