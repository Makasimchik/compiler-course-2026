// RUN: %clang_cc1 -load %llvmshlibdir/titaev_m_lab1_ClangAST%pluginext -plugin deprecated_function_checker -fsyntax-only -verify %s

void deprecated_declared_only();

void deprecated_function() {}
// expected-warning@-1 {{function 'deprecated_function' contains 'deprecated' in its name}}

int my_deprecated_api(int value) {
  return value;
}
// expected-warning@-3 {{function 'my_deprecated_api' contains 'deprecated' in its name}}

void normal_function() {}

int deprecated_inside_name() {
  return 42;
}
// expected-warning@-3 {{function 'deprecated_inside_name' contains 'deprecated' in its name}}