// RUN: %clang_cc1 -load %llvmshlibdir/titaev_m_lab1_extra_task_ClangAST%pluginext -plugin deprecated_function_checker -fsyntax-only -verify %s

void deprecated_declared_only();

void deprecated_function() {} // expected-warning {{function 'deprecated_function' contains 'deprecated' in its name}}

int my_deprecated_api(int value) { // expected-warning {{function 'my_deprecated_api' contains 'deprecated' in its name}}
  return value;
}

void normal_function() {}

int deprecated_inside_name() { // expected-warning {{function 'deprecated_inside_name' contains 'deprecated' in its name}}
  return 42;
}