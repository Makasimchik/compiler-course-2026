// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/telnov_lab4_MLIR%shlibext --pass-pipeline="builtin.module(telnov-trip-count)" %s | FileCheck %s

// CHECK-LABEL: func.func @plain_static_loop
// CHECK: affine.for %{{.*}} = 1 to 9 {
// CHECK: } {trip_count = 8 : i64}
func.func @plain_static_loop() {
  affine.for %i = 1 to 9 {
  }
  return
}

// CHECK-LABEL: func.func @stepped_loop_exact
// CHECK: affine.for %{{.*}} = 3 to 15 step 4 {
// CHECK: } {trip_count = 3 : i64}
func.func @stepped_loop_exact() {
  affine.for %i = 3 to 15 step 4 {
  }
  return
}

// CHECK-LABEL: func.func @stepped_loop_rounded_up
// CHECK: affine.for %{{.*}} = 2 to 13 step 5 {
// CHECK: } {trip_count = 3 : i64}
func.func @stepped_loop_rounded_up() {
  affine.for %i = 2 to 13 step 5 {
  }
  return
}

// CHECK-LABEL: func.func @below_zero_start
// CHECK: affine.for %{{.*}} = -5 to 2 step 3 {
// CHECK: } {trip_count = 3 : i64}
func.func @below_zero_start() {
  affine.for %i = -5 to 2 step 3 {
  }
  return
}

// CHECK-LABEL: func.func @empty_iteration_range
// CHECK: affine.for %{{.*}} = 7 to 7 {
// CHECK: } {trip_count = 0 : i64}
func.func @empty_iteration_range() {
  affine.for %i = 7 to 7 {
  }
  return
}

// CHECK-LABEL: func.func @two_level_affine_loop
// CHECK: affine.for %{{.*}} = 0 to 3 {
// CHECK: affine.for %{{.*}} = 4 to 14 step 4 {
// CHECK: } {trip_count = 3 : i64}
// CHECK: } {trip_count = 3 : i64}
func.func @two_level_affine_loop() {
  affine.for %i = 0 to 3 {
    affine.for %j = 4 to 14 step 4 {
    }
  }
  return
}

// CHECK-LABEL: func.func @runtime_upper_bound
// CHECK: affine.for %{{.*}} = 0 to %{{.*}} {
// CHECK-NOT: trip_count
// CHECK: return
func.func @runtime_upper_bound(%n: index) {
  affine.for %i = 0 to %n {
  }
  return
}

// CHECK-LABEL: func.func @already_has_trip_count
// CHECK: affine.for %{{.*}} = 0 to 10 {
// CHECK: } {trip_count = 42 : i64}
func.func @already_has_trip_count() {
  affine.for %i = 0 to 10 {
  } {trip_count = 42 : i64}
  return
}