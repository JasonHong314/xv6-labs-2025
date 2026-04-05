#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void memdump(char *fmt, char *data);

int
main(int argc, char *argv[])
{
  if(argc == 1){
    printf("Example 1:\n");
    int a[2] = { 61810, 2025 };
    memdump("ii", (char*) a);
    
    printf("Example 2:\n");
    memdump("S", "a string");
    
    printf("Example 3:\n");
    char *s = "another";
    memdump("s", (char *) &s);

    struct sss {
      char *ptr;
      int num1;
      short num2;
      char byte;
      char bytes[8];
    } example;
    
    example.ptr = "hello";
    example.num1 = 1819438967;
    example.num2 = 100;
    example.byte = 'z';
    strcpy(example.bytes, "xyzzy");
    
    printf("Example 4:\n");
    memdump("pihcS", (char*) &example);
    
    printf("Example 5:\n");
    memdump("sccccc", (char*) &example);
  } else if(argc == 2){
    // format in argv[1], up to 512 bytes of data from standard input.
    char data[512];
    int n = 0;
    memset(data, '\0', sizeof(data));
    while(n < sizeof(data)){
      int nn = read(0, data + n, sizeof(data) - n);
      if(nn <= 0)
        break;
      n += nn;
    }
    memdump(argv[1], data);
  } else {
    printf("Usage: memdump [format]\n");
    exit(1);
  }
  exit(0);
}

void
memdump(char *fmt, char *data)
{
  for(int i = 0; fmt[i] != '\0'; i++){
    switch(fmt[i]){

      case 'i':
        // 读 4 字节 → 当成 int
        printf("%d\n", *(int*)data);
        data += 4; 
        break;

      case 'p':
        // 读 8 字节 → 当成 64 位十六进制数
        printf("%lx\n", *(uint64*)data);
        data += 8;
        break;

      case 'h':
        // 读 2 字节 → 当成 short
        printf("%d\n", *(short*)data);
        data += 2;
        break;

      case 'c':
        // 读 1 字节 → 当成字符
        printf("%c\n", *(char*)data);
        data += 1;
        break;

      case 's':
        // 读 8 字节 → 是一个字符串指针
        printf("%s\n", *(char**)data);
        data += 8;
        break;

      case 'S':
        // 剩下所有数据 → 直接当字符串输出
        printf("%s\n", data);
        // 跳到字符串末尾（结束）
        while(*data != '\0') data++;
        data++;
        break;
    }
  }
}
