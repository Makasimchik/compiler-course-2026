// RUN: mlir-opt -load-pass-plugin=%mlir_lib_dir/KiselevILastLab_MLIR%shlibext --pass-pipeline="builtin.module(kiselev-cycle-merging)" %s | FileCheck %s

// CHECK-LABEL: func.func @good_case()
// CHECK: scf.for %{{.*}} = %c0 to %c10 step %c1
// CHECK: arith.addi
// CHECK: arith.muli
// CHECK-NOT: scf.for %{{.*}} = %c0 to %c10 step %c1

module {
  func.func @good_case() {
    %c0 = arith.constant 0 : index
    %c10 = arith.constant 10 : index
    %c1 = arith.constant 1 : index

    scf.for %i = %c0 to %c10 step %c1 {
      %x = arith.addi %i, %i : index
    }

    scf.for %i = %c0 to %c10 step %c1 {
      %y = arith.muli %i, %i : index
    }

    return
  }
}

// CHECK-LABEL: func.func @bounds_problem()
// CHECK-COUNT-2: scf.for

module {
  func.func @bounds_problem() {
    %c0 = arith.constant 0 : index
    %c10 = arith.constant 10 : index
    %c20 = arith.constant 20 : index
    %c1 = arith.constant 1 : index

    scf.for %i = %c0 to %c10 step %c1 {
    }

    scf.for %i = %c0 to %c20 step %c1 {
    }

    return
  }
}

// CHECK-LABEL: func.func @deps()
// CHECK-COUNT-2: scf.for

module {
  func.func @deps() {
    %c0 = arith.constant 0 : index
    %c10 = arith.constant 10 : index
    %c1 = arith.constant 1 : index

    scf.for %i = %c0 to %c10 step %c1 {
      %tmp = arith.addi %i, %i : index
      "test.use"(%tmp) : (index) -> ()
    }

    scf.for %i = %c0 to %c10 step %c1 {
      %x = arith.addi %i, %i : index
    }

    return
  }
}

// CHECK-LABEL: func.func @diff_step()
// CHECK-COUNT-2: scf.for

module {
  func.func @diff_step() {
    %c0 = arith.constant 0 : index
    %c10 = arith.constant 10 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index

    scf.for %i = %c0 to %c10 step %c1 {
    }

    scf.for %i = %c0 to %c10 step %c2 {
    }

    return
  }
}

// CHECK-LABEL: func.func @not_neighbour()
// CHECK-COUNT-2: scf.for

module {
  func.func @not_neighbour() {
    %c0 = arith.constant 0 : index
    %c10 = arith.constant 10 : index
    %c1 = arith.constant 1 : index

    scf.for %i = %c0 to %c10 step %c1 {
    }

    %tmp = arith.constant 5 : index   // мешает

    scf.for %i = %c0 to %c10 step %c1 {
    }

    return
  }
}