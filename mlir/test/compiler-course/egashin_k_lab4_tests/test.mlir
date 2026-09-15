// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/egashin_k_lab4_MLIR%shlibext --pass-pipeline="builtin.module(annotate-affine-trip-count)" %s | FileCheck %s

// CHECK-LABEL: func.func @static_simple()
// CHECK: trip_count = 4 : i64
func.func @static_simple() {
  affine.for %i = 0 to 8 step 2 {
  }
  return
}

// CHECK-LABEL: func.func @zero_trip()
// CHECK: trip_count = 0 : i64
func.func @zero_trip() {
  affine.for %i = 5 to 5 {
  }
  return
}

// CHECK-LABEL: func.func @nested_known()
// CHECK: affine.for
// CHECK: affine.for
// CHECK: } {trip_count = 3 : i64}
// CHECK: } {trip_count = 2 : i64}
func.func @nested_known() {
  affine.for %i = 0 to 2 {
    affine.for %j = 0 to 6 step 2 {
    }
  }
  return
}

// CHECK-LABEL: func.func @dynamic_unknown
// CHECK-NOT: trip_count =
// CHECK: return
func.func @dynamic_unknown(%n: index) {
  affine.for %i = 0 to %n {
  }
  return
}
