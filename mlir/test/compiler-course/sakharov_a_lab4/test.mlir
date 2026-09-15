// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/sakharov_a_lab4_MLIR%shlibext --pass-pipeline="builtin.module(sakharov-trip-count)" %s | FileCheck %s

// CHECK-LABEL: func.func @constant_bounds
// CHECK: affine.for %{{.*}} = 0 to 10 {
// CHECK: } {trip_count = 10 : i64}
func.func @constant_bounds(%arg0: memref<20xi32>) {
  affine.for %i = 0 to 10 {
    %0 = affine.load %arg0[%i] : memref<20xi32>
  }
  return
}

// CHECK-LABEL: func.func @constant_bounds_with_step
// CHECK: affine.for %{{.*}} = 2 to 11 step 3 {
// CHECK: } {trip_count = 3 : i64}
func.func @constant_bounds_with_step(%arg0: memref<20xi32>) {
  affine.for %i = 2 to 11 step 3 {
    %0 = affine.load %arg0[%i] : memref<20xi32>
  }
  return
}

// CHECK-LABEL: func.func @ceil_div_trip_count
// CHECK: affine.for %{{.*}} = 0 to 10 step 3 {
// CHECK: } {trip_count = 4 : i64}
func.func @ceil_div_trip_count(%arg0: memref<20xi32>) {
  affine.for %i = 0 to 10 step 3 {
    %0 = affine.load %arg0[%i] : memref<20xi32>
  }
  return
}

// CHECK-LABEL: func.func @negative_lower_bound
// CHECK: affine.for %{{.*}} = -3 to 4 step 2 {
// CHECK: } {trip_count = 4 : i64}
func.func @negative_lower_bound() {
  affine.for %i = -3 to 4 step 2 {
  }
  return
}

// CHECK-LABEL: func.func @zero_iterations
// CHECK: affine.for %{{.*}} = 5 to 5 {
// CHECK: } {trip_count = 0 : i64}
func.func @zero_iterations(%arg0: memref<20xi32>) {
  affine.for %i = 5 to 5 {
    %0 = affine.load %arg0[%i] : memref<20xi32>
  }
  return
}

// CHECK-LABEL: func.func @nested_loops
// CHECK: affine.for %{{.*}} = 0 to 4 {
// CHECK: affine.for %{{.*}} = 0 to 8 step 2 {
// CHECK: } {trip_count = 4 : i64}
// CHECK: } {trip_count = 4 : i64}
func.func @nested_loops(%arg0: memref<20x20xi32>) {
  affine.for %i = 0 to 4 {
    affine.for %j = 0 to 8 step 2 {
      %0 = affine.load %arg0[%i, %j] : memref<20x20xi32>
    }
  }
  return
}

// CHECK-LABEL: func.func @unknown_trip_count
// CHECK: affine.for %{{.*}} = 0 to %{{.*}} {
// CHECK-NOT: trip_count
// CHECK: return
func.func @unknown_trip_count(%arg0: index, %arg1: memref<?xi32>) {
  affine.for %i = 0 to %arg0 {
    %0 = affine.load %arg1[%i] : memref<?xi32>
  }
  return
}

// CHECK-LABEL: func.func @existing_trip_count_is_preserved
// CHECK: affine.for %{{.*}} = 0 to 10 {
// CHECK: } {trip_count = 123 : i64}
func.func @existing_trip_count_is_preserved(%arg0: memref<20xi32>) {
  affine.for %i = 0 to 10 {
    %0 = affine.load %arg0[%i] : memref<20xi32>
  } {trip_count = 123 : i64}
  return
}
