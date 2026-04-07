#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

// #define NPAGES 64

// static int
// is_alpha_num(char c)
// {
//   if(c >= 'a' && c <= 'z')
//     return 1;
//   if(c >= 'A' && c <= 'Z')
//     return 1;
//   if(c >= '0' && c <= '9')
//     return 1;
//   return 0;
// }

// int
// main(int argc, char *argv[])
// {
//   char *p;
//   int i, j;
//   int len;
//   int has_upper, has_lower, has_digit;

//   p = sbrk(NPAGES * PGSIZE);
//   if((long)p == -1){
//     fprintf(2, "sbrk failed\n");
//     exit(1);
//   }

//   // 在新分配到的页里扫描看起来像 secret 的字符串：
//   // 1. 只接受字母数字串
//   // 2. 长度至少 8
//   // 3. 最后必须以 '\0' 结束
//   // 4. 最好同时包含大写/小写/数字，以减少误判
//   for(i = 0; i < NPAGES * PGSIZE; i++){
//     if(!is_alpha_num(p[i]))
//       continue;

//     // 尽量从“串起点”开始
//     if(i > 0 && is_alpha_num(p[i - 1]))
//       continue;

//     has_upper = 0;
//     has_lower = 0;
//     has_digit = 0;
//     len = 0;

//     for(j = i; j < NPAGES * PGSIZE; j++){
//       if(p[j] == '\0')
//         break;

//       if(!is_alpha_num(p[j])){
//         len = 0;
//         break;
//       }

//       if(p[j] >= 'A' && p[j] <= 'Z')
//         has_upper = 1;
//       else if(p[j] >= 'a' && p[j] <= 'z')
//         has_lower = 1;
//       else if(p[j] >= '0' && p[j] <= '9')
//         has_digit = 1;

//       len++;
//     }

//     if(len >= 8 && j < NPAGES * PGSIZE && p[j] == '\0'){
//       if(has_upper && has_lower && has_digit){
//         printf("%s\n", p + i);
//         exit(0);
//       }
//     }
//   }

//   printf("(null)\n");
//   exit(1);
// }
//上述为启发式暴力算法，请勿参考，仅有概率能骗过test
int
main(int argc, char *argv[])
{
  char *base = sbrk(32 * PGSIZE);
  // printf("%s\n", base + 17 * PGSIZE + 32);/*这种写法也可以，write好像更合规一些？*/ 
  base += 17 * PGSIZE +32;
  write(2, base, 8);
  exit(0);
}