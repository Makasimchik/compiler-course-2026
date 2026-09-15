// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/agafonov_i_lab4_MLIR%shlibext --pass-pipeline="builtin.module(add-trip-count)" %s | FileCheck %s

// CHECK-LABEL: func.func @simple_loop
func.func @simple_loop() {
  // CHECK: affine.for %{{.*}} = 0 to 10 {
  affine.for %arg0 = 0 to 10 {
    "test.noop"() : () -> ()
  // CHECK: } {trip_count = 10 : i64}
  }
  return
}

// CHECK-LABEL: func.func @offset_loop
func.func @offset_loop() {
  // CHECK: affine.for %{{.*}} = 5 to 15 {
  affine.for %arg0 = 5 to 15 {
    "test.noop"() : () -> ()
  // CHECK: } {trip_count = 10 : i64}
  }
  return
}

// CHECK-LABEL: func.func @step_loop
func.func @step_loop() {
  // CHECK: affine.for %{{.*}} = 0 to 10 step 2 {
  affine.for %arg0 = 0 to 10 step 2 {
    "test.noop"() : () -> ()
  // CHECK: } {trip_count = 5 : i64}
  }
  return
}

// CHECK-LABEL: func.func @nested_loops
func.func @nested_loops() {
  // CHECK: affine.for %{{.*}} = 0 to 4 {
  affine.for %arg0 = 0 to 4 {
    // CHECK: affine.for %{{.*}} = 0 to 8 {
    affine.for %arg1 = 0 to 8 {
      "test.noop"() : () -> ()
    // CHECK: } {trip_count = 8 : i64}
    }
  // CHECK: } {trip_count = 4 : i64}
  }
  return
}

// CHECK-LABEL: func.func @dynamic_loop
func.func @dynamic_loop(%arg0: index) {
  // CHECK: affine.for %{{.*}} = 0 to %{{.*}} {
  // CHECK-NOT: trip_count
  affine.for %arg1 = 0 to %arg0 {
    "test.noop"() : () -> ()
  }
  return
}

// CHECK-LABEL: func.func @tricky_step_loop
func.func @tricky_step_loop() {
  // CHECK: affine.for %{{.*}} = 1 to 10 step 3 {
  affine.for %arg0 = 1 to 10 step 3 {
    "test.noop"() : () -> ()
  // CHECK: } {trip_count = 3 : i64}
  }
  return
}

// CHECK-LABEL: func.func @zero_iterations
func.func @zero_iterations() {
  // CHECK: affine.for %{{.*}} = 20 to 10 {
  affine.for %arg0 = 20 to 10 {
    "test.noop"() : () -> ()
  // CHECK: } {trip_count = 0 : i64}
  }
  return
}