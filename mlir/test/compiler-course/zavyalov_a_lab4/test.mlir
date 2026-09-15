// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/zavyalov_a_lab4_MLIR%shlibext --pass-pipeline="builtin.module(AffineTripCount_MLIR)" %s | FileCheck %s

func.func @test_loops() {
  affine.for %i = 0 to 10 {
    affine.for %j = 0 to 8 step 3 {
    // CHECK: } {trip_count = 3 : i64}
    }
  // CHECK: } {trip_count = 10 : i64}
  }

  affine.for %i = 0 to 4 {
    affine.for %j = 0 to 4 {
      affine.for %k = 0 to 4 {
      // CHECK: } {trip_count = 4 : i64}
      }
    // CHECK: } {trip_count = 4 : i64}
    }
  // CHECK: } {trip_count = 4 : i64}
  }

  affine.for %i = 10 to 5 {
  // CHECK: } {trip_count = 0 : i64}
  }

  affine.for %i = 5 to 5 {
  // CHECK: } {trip_count = 0 : i64}
  }

  affine.for %i = 0 to 3 step 10 {
  // CHECK: } {trip_count = 1 : i64}
  }

  %N = arith.constant 5 : index
  affine.for %k = 0 to %N {
  // CHECK-NOT: trip_count
  }

  return
}