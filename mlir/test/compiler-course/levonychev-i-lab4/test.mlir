// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/levonychev-i-lab4_MLIR%shlibext --pass-pipeline="builtin.module(levonychev_MLIR)" %s | FileCheck %s

// CHECK-LABEL: func @known_lower_upper_bound
func.func @known_lower_upper_bound() {
  // CHECK: affine.for %{{.*}} = 0 to 42
  // CHECK: {trip_count = 42 : index}
  affine.for %i = 0 to 42 {
    affine.yield
  }
  return
}

// CHECK-LABEL: func @unknown_lower_bound
func.func @unknown_lower_bound(%arg0: index) {
  // CHECK: affine.for %{{.*}} = %arg0 to 42
  // CHECK-NOT: trip_count
  affine.for %i = %arg0 to 42 {
    affine.yield
  }
  return
}

// CHECK-LABEL: func @unknown_upper_bound
func.func @unknown_upper_bound(%arg0: index) {
  // CHECK: affine.for %{{.*}} = 0 to %arg0
  // CHECK-NOT: trip_count
  affine.for %i = 0 to %arg0 {
    affine.yield
  }
  return
}

// CHECK-LABEL: func @unknown_lower_upper_bound_param
func.func @unknown_lower_upper_bound_param(%arg0: index, %arg1: index) {
  // CHECK: affine.for %{{.*}} = %arg0 to %arg1
  // CHECK-NOT: trip_count
  affine.for %i = %arg0 to %arg1 {
    affine.yield
  }
  return
}

// CHECK-LABEL: func @large_step
func.func @large_step() {
  // CHECK: affine.for %{{.*}} = 1 to 10 step 100
  // CHECK: {trip_count = 1 : index}
  affine.for %i = 1 to 10 step 100 {
    affine.yield
  }
  return
}

// CHECK-LABEL: func @zero_iterations
func.func @zero_iterations() {
  // CHECK: affine.for %{{.*}} = 5 to 5
  // CHECK: {trip_count = 0 : index}
  affine.for %i = 5 to 5 {
    affine.yield
  }
  return
}

// CHECK-LABEL: func @nested_loops
func.func @nested_loops() {
  // CHECK: affine.for %{{.*}} = 0 to 10 {
  affine.for %i = 0 to 10 {
    // CHECK: affine.for %{{.*}} = 0 to 5 {
    affine.for %j = 0 to 5 {
      affine.yield
    }
    // CHECK: } {trip_count = 5 : index}
    affine.yield
  }
  // CHECK: } {trip_count = 10 : index}
  return
}

// CHECK-LABEL: func @negative_range
func.func @negative_range() {
  // CHECK: affine.for %{{.*}} = 10 to 0
  // CHECK: {trip_count = 0 : index}
  affine.for %i = 10 to 0 {
    affine.yield
  }
  return
}

// CHECK-LABEL: func @non_divisible_step
func.func @non_divisible_step() {
  // CHECK: affine.for %{{.*}} = 0 to 10 step 3
  // CHECK: {trip_count = 4 : index}
  affine.for %i = 0 to 10 step 3 {
    affine.yield
  }
  return
}