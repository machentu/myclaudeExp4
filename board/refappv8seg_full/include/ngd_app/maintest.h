#ifndef MAIN_CPP_H
#define MAIN_CPP_H

// 只有在 C++ 编译器编译时，才启用 extern "C"
#ifdef __cplusplus
extern "C" {
#endif

// 这里声明供 C 语言调用的函数
int maintest(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif // MAIN_CPP_H
