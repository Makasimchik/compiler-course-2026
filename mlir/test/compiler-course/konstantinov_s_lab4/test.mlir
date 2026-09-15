// RUN: mlir-opt
// -load-pass-plugin=%mlir_lib_dir/konstantinov_s_lab4_MLIR%shlibext \ RUN:
// --pass-pipeline="builtin.module(func.func(konstantinov_s_lab4_MLIR))" %s |
// FileCheck %s

// --- 0 ---

// CHECK-LABEL: func.func @just_function
// CHECK-SAME: max_block_depth = 0 : i64
func.func @just_function(% x : i64, % y : i64)
    ->i64{ % result = arith.muli % x, % y : i64 return % result : i64}

// --- affine.for 1 ---

// CHECK-LABEL: func.func @affine_for
// CHECK-SAME: max_block_depth = 1 : i64
func.func @affine_for() {
  affine.for %idx = 0 to 5 {
  }
  return
}

// --- affine.if 1 ---

// CHECK-LABEL: func.func @affine_if
// CHECK-SAME: max_block_depth = 1 : i64
func.func @affine_if(% coord : index) {
  affine.if affine_set<(d0) : (d0 <= 100)>(% coord) {}
  return
}

// --- scf.if (no else) 1 ---

// CHECK-LABEL: func.func @if_no_else
// CHECK-SAME: max_block_depth = 1 : i64
func.func @if_no_else(% condition : i1) {
  scf.if % condition {}
  return
}

// --- scf.for 1 ---

// CHECK-LABEL: func.func @scf_for
// CHECK-SAME: max_block_depth = 1 : i64
func.func @scf_for(% start : index, % end : index, % stride : index) {
  scf.for %idx = %start to %end step %stride {
  }
  return
}

// --- scf.if else 1 ---

// CHECK-LABEL: func.func @if_with_else
// CHECK-SAME: max_block_depth = 1 : i64
func.func @if_with_else(% predicate : i1)->i32 {
  % value = scf.if % predicate->i32 {
    % const5 = arith.constant 5 : i32 scf.yield % const5 : i32
  }
  else {
    % const3 = arith.constant 3 : i32 scf.yield % const3 : i32
  }
  return % value : i32
}

// --- scf.while 1 ---

// CHECK-LABEL: func.func @single_while
// CHECK-SAME: max_block_depth = 1 : i64
func.func @single_while(% initial : i32, % max_val : i32)->i32 {
  % result = scf.while (% current = % initial) : (i32)->i32 {
    % is_less = arith.cmpi ult, % current,
      % max_val : i32 scf.condition(% is_less) % current : i32
  }
  do {
    ^bb0(% current : i32)
        : %
        two = arith.constant 2 : i32 % next_val = arith.subi % current,
        % two : i32 scf.yield % next_val : i32
  }
  return % result : i32
}

// --- two sequential ifs 1 ---

// CHECK-LABEL: func.func @two_ifs_sequential
// CHECK-SAME: max_block_depth = 1 : i64
func.func @two_ifs_sequential(% cond1 : i1, % cond2 : i1) {
  scf.if % cond1{ % dummy1 = arith.constant 42 : i32} scf.if % cond2 {
    % dummy2 = arith.constant 43 : i32
  }
  return
}

// --- for with nested while 2 ---

// CHECK-LABEL: func.func @for_with_while_nested
// CHECK-SAME: max_block_depth = 2 : i64
func.func @for_with_while_nested(% lb : index, % ub : index, % step : index) {
  scf.for %i = %lb to %ub step %step {
    % ignored = scf.while (% cur = % i) : (index)->index {
      % cond = arith.cmpi slt, % cur,
        % ub : index scf.condition(% cond) % cur : index
    }
    do {
      ^bb0(% cur : index)
          : %
          next = arith.addi % cur,
          % step : index scf.yield % next : index
    }
  }
  return
}

// --- while with nested if 2 ---

// CHECK-LABEL: func.func @while_with_if_nested
// CHECK-SAME: max_block_depth = 2 : i64
func.func @while_with_if_nested(% init : index, % limit : index, % flag : i1) {
  % ignored = scf.while (% cur = % init) : (index)->index {
    % cond = arith.cmpi slt, % cur,
      % limit : index scf.condition(% cond) % cur : index
  }
  do {
    ^bb0(% cur : index)
        : scf.if %
        flag{
            % dummy = arith.constant 1 : index} %
        one = arith.constant 1 : index % next = arith.addi % cur,
        % one : index scf.yield % next : index
  }
  return
}

// --- if with nested while 2 ---

// CHECK-LABEL: func.func @if_with_while_nested
// CHECK-SAME: max_block_depth = 2 : i64
func.func @if_with_while_nested(% cond : i1, % start : index, % limit : index) {
  scf.if % cond {
    % ignored = scf.while (% cur = % start) : (index)->index {
      % cmp = arith.cmpi ne, % cur,
        % limit : index scf.condition(% cmp) % cur : index
    }
    do {
      ^bb0(% cur : index)
          : %
          next = arith.subi % cur,
          % start : index scf.yield % next : index
    }
  }
  return
}

// --- if with while in else 2 ---

// CHECK-LABEL: func.func @if_with_while_in_else
// CHECK-SAME: max_block_depth = 2 : i64
func.func @if_with_while_in_else(% flag : i1, % init : index, % bound : index) {
  scf.if % flag { % c100 = arith.constant 100 : index }
  else {
    % ignored = scf.while (% cur = % init) : (index)->index {
      % cond = arith.cmpi sgt, % cur,
        % bound : index scf.condition(% cond) % cur : index
    }
    do {
      ^bb0(% cur : index)
          : %
          dec = arith.constant 1 : index % next = arith.subi % cur,
          % dec : index scf.yield % next : index
    }
  }
  return
}

// --- complicated nesting 4 ---

// CHECK-LABEL: func.func @complex_nesting_4
// CHECK-SAME: max_block_depth = 4 : i64
func.func @complex_nesting_4(% lb : index, % ub : index, % step : index,
                             % flag : i1) {
  scf.if % flag {
    scf.for %i = %lb to %ub step %step {
      % ignored = scf.while (% cur = % i) : (index)->index {
        % cond = arith.cmpi ne, % cur,
          % ub : index scf.condition(% cond) % cur : index
      }
      do {
        ^bb0(%cur: index):
        scf.for %j = %lb to %cur step %step {
          %c7 = arith.constant 7 : index
        }
        %next_idx = arith.addi %cur, %step : index
        scf.yield %next_idx : index
      }
    }
  }
  else {
    % ignored = scf.while (% w = % lb) : (index)->index {
      % cnd = arith.cmpi ult, % w, % ub : index scf.condition(% cnd) % w : index
    }
    do {
      ^bb0(% w : index) : % c0 = arith.constant 0 : index scf.yield % c0 : index
    }
  }
  return
}

// --- three nested ifs and for 3 ---

// CHECK-LABEL: func.func @nested_ifs_and_for
// CHECK-SAME: max_block_depth = 3 : i64
func.func @nested_ifs_and_for(% cond1 : i1, % cond2 : i1, % lb : index,
                              % ub : index, % step : index) {
  scf.if % cond1 {
    scf.if % cond2 {
      scf.for %i = %lb to %ub step %step {
        % c99 = arith.constant 99 : index
      }
    }
  }
  return
}

// --- affine if with for inside 2 ---

// CHECK-LABEL: func.func @affine_if_with_for
// CHECK-SAME: max_block_depth = 2 : i64
func.func @affine_if_with_for(% idx : index) {
  affine.if affine_set<(d0) : (d0 >= 0, d0 <= 5)>(% idx) {
    affine.for %i = 0 to 10 {
      %dummy = arith.constant 1 : i32
    }
  }
  return
}

// --- for with nested if 2 ---

// CHECK-LABEL: func.func @for_with_if
// CHECK-SAME: max_block_depth = 2 : i64
func.func
    @for_with_if(% lb : index, % ub : index, % step : index, % cond : i1) {
  scf.for %i = %lb to %ub step %step {
    scf.if % cond { % c5 = arith.constant 5 : index }
  }
  return
}

// --- while with nested for 2 ---

// CHECK-LABEL: func.func @while_with_for
// CHECK-SAME: max_block_depth = 2 : i64
func.func @while_with_for(% init : index, % limit : index, % lb : index,
                          % ub : index, % step : index) {
  % ignored = scf.while (% cur = % init) : (index)->index {
    % cond = arith.cmpi slt, % cur,
      % limit : index scf.condition(% cond) % cur : index
  }
  do {
    ^bb0(%cur: index):
    scf.for %i = %lb to %ub step %step {
      %dummy = arith.constant 10 : index
    }
    %next = arith.addi %cur, %step : index
    scf.yield %next : index
  }
  return
}

// --- code around loop 1 ---

// CHECK-LABEL: func.func @code_around_loop
// CHECK-SAME: max_block_depth = 1 : i64
func.func @code_around_loop(% lower_bound : index, % upper_bound : index,
                            % step_count : index)
    ->i32 {
  %x = arith.constant 100 : i32
  scf.for %loop_var = %lower_bound to %upper_bound step %step_count {
  }
  %y = arith.constant 200 : i32
  %result = arith.subi %x, %y : i32
  return %result : i32
}