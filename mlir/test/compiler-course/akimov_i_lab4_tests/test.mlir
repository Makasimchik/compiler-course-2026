// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/akimov_i_lab4_MLIR%shlibext --pass-pipeline="builtin.module(annotate-trip-count)" %s | FileCheck %s

// -----
// CHECK-LABEL: func.func @constant_trip_count
// CHECK: affine.for %{{.*}} = 0 to 10 step 2 {
// CHECK-NEXT: } {trip_count = 5 : i64}
func.func @constant_trip_count() {
  affine.for %i = 0 to 10 step 2 {
    // nothing
  }
  return
}

// -----
// CHECK-LABEL: func.func @unknown_trip_count
// CHECK: affine.for %{{.*}} = 0 to %{{.*}} {
// CHECK-NEXT: }
// CHECK-NOT: trip_count
func.func @unknown_trip_count(%N : index) {
  affine.for %i = 0 to %N {
    // nothing
  }
  return
}

// -----
// CHECK-LABEL: func.func @simple_loop
// CHECK: affine.for %{{.*}} = 0 to 10 {
// CHECK-NEXT: } {trip_count = 10 : i64}
func.func @simple_loop() {
  affine.for %i = 0 to 10 {
    // nothing
  }
  return
}

// -----
// CHECK-LABEL: func.func @mixed
// CHECK: affine.for %{{.*}} = 0 to 5 {
// CHECK-NEXT: } {trip_count = 5 : i64}
// CHECK: affine.for %{{.*}} = 0 to %{{.*}} {
// CHECK-NEXT: }
// CHECK-NOT: trip_count
// CHECK: affine.for %{{.*}} = 0 to 20 step 4 {
// CHECK-NEXT: } {trip_count = 5 : i64}
func.func @mixed(%M : index) {
  affine.for %i = 0 to 5 {
  }
  affine.for %j = 0 to %M {
  }
  affine.for %k = 0 to 20 step 4 {
  }
  return
}

// -----
// CHECK-LABEL: func.func @non_divisible_step
// CHECK: affine.for %{{.*}} = 0 to 10 step 3 {
// CHECK-NEXT: } {trip_count = 4 : i64}
func.func @non_divisible_step() {
  affine.for %i = 0 to 10 step 3 {
  }
  return
}
