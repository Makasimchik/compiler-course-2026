// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/MakovskiiAffinePass%shlibext --pass-pipeline="builtin.module(affine-annotate-trip-count)" %s | FileCheck %s

// CHECK-LABEL: func.func @test_static
func.func @test_static() {
  // CHECK: affine.for %{{.*}} = 0 to 16 {
  // CHECK: } {trip_count = 16 : i64}
  affine.for %i = 0 to 16 {
    %0 = arith.constant 1 : i32
  }
  return
}

// CHECK-LABEL: func.func @test_step
func.func @test_step() {
  // CHECK: affine.for %{{.*}} = 0 to 10 step 3 {
  // CHECK: } {trip_count = 4 : i64}
  affine.for %i = 0 to 10 step 3 {
    %0 = arith.constant 1 : i32
  }
  return
}

// CHECK-LABEL: func.func @test_dynamic
func.func @test_dynamic(%arg0: index) {
  // CHECK: affine.for %{{.*}} = 0 to %{{.*}} {
  // CHECK-NOT: trip_count
  affine.for %i = 0 to %arg0 {
    %0 = arith.constant 1 : i32
  }
  return
}

// CHECK-LABEL: func.func @test_nested
func.func @test_nested() {
  // CHECK: affine.for %{{.*}} = 2 to 10 {
  // CHECK:   affine.for %{{.*}} = 0 to 4 step 2 {
  // CHECK:   } {trip_count = 2 : i64}
  // CHECK: } {trip_count = 8 : i64}
  affine.for %i = 2 to 10 {
    affine.for %j = 0 to 4 step 2 {
      %0 = arith.constant 1 : i32
    }
  }
  return
}

// CHECK-LABEL: func.func @test_zero_iterations
func.func @test_zero_iterations() {
  // CHECK: affine.for %{{.*}} = 10 to 0 {
  // CHECK: } {trip_count = 0 : i64}
  affine.for %i = 10 to 0 {
    %0 = arith.constant 1 : i32
  }
  return
}