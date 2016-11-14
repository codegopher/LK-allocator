#define CEILING_POS(X) ((X-(int)(X)) > 0 ? (int)(X+1) : (int)(X))
#define CEILING_NEG(X) ((X-(int)(X)) < 0 ? (int)(X-1) : (int)(X))
#define CEILING(X) ( ((X) > 0) ? CEILING_POS(X) : CEILING_NEG(X) )

#include <unistd.h>
#include <sys/syscall.h>
#include <string.h> // memset, memcpy
#include <errno.h>

// gcc -fpic -shared -g allocator.c -o alloc && LD_PRELOAD=./alloc ls - тест
// Работают: ls
// Не работают: ping(out of memory с errno), ps (выводится только первый процесс)

// Все переопределяемые функции изначально определены в stdlib.h

void *seg_start; // Начало и конец сегмента данных приложения
void *seg_end;
char wasinit = 0; // флаг инициализированности. Иначе просто не вышло
extern int errno; // Оно же так работает,правда?

// Будем писать информацию о блоке непосредственно перед ним
struct block_inf{
  char avaliable;
  size_t size;
};
int inf_offset = sizeof(struct block_inf); // Размер блока, чтобы постоянно не дергать sizeof

void str_syswrite(char *str) {syscall(SYS_write,1,str,strlen(str));} // Лень писать syscall везде

void int_syswrite(int var) {

  char str[9];
  char *hex = "0123456789abcde";

  str[0] = hex[(var >> 28) & 0xF];
  str[1] = hex[(var >> 24) & 0xF];
  str[2] = hex[(var >> 20) & 0xF];
  str[3] = hex[(var >> 16) & 0xF];
  str[4] = hex[(var >> 12) & 0xF];
  str[5] = hex[(var >> 8) & 0xF];
  str[6] = hex[(var >> 4) & 0xF];
  str[7] = hex[(var >> 0) & 0xF];
  str[8] ='\0';

  str_syswrite(str);
}

void ptr_syswrite(void *ptr) { int_syswrite((int)ptr);}

void init(){
  wasinit = 1;
  seg_end = sbrk(0); // Последний доступный - в хвосте процесса
  seg_start = seg_end;
  ptr_syswrite(seg_end);
  str_syswrite("-tail ");
  int_syswrite(inf_offset);
  str_syswrite("-struct size ");
  int_syswrite(errno);
  str_syswrite("-initerrno \n"); // errno = 2 изначально, но не всегда. WTF?
  errno = 0;
}

void* malloc(size_t size){
  if (wasinit == 0) {init();} // увы, пришлось так
  str_syswrite(" malloc: ");
    int_syswrite(errno);
    str_syswrite("-errno_mstart ");
  size = size + inf_offset;
  int allocsize = CEILING(size/4.0) << 2; // Выравниваем
  void* current_pos = seg_start; // Текущее "местонахождение"
  void* mem_to_return; // Что вернем
  char found = 0; // Подходящий блок найден
  struct block_inf *current_inf; // Информация о текущем блоке
  int_syswrite(errno);
  str_syswrite("-errno_wstart ");
  int_syswrite(allocsize);
  str_syswrite("-size ");

  while ( (current_pos != seg_end) && (found == 0)){
    current_inf = current_pos; // Информация о текущем блоке
    if ( (current_inf->size >= allocsize) &&  (current_inf->avaliable == 1)) {
    // Если блок свободен и подходит по размеру, используем его
      found = 1;
      mem_to_return = current_pos;
      str_syswrite(" using existing block, ");
    }
    current_pos = current_pos + current_inf->size;
    str_syswrite("\n loop: ");
    int_syswrite(current_inf->size);
    str_syswrite("-cursize "); // Для отладки
    ptr_syswrite(current_inf);
    str_syswrite("-curpos ");
    int_syswrite(errno);
    str_syswrite("-errno_wend ");
  }

  if (found == 0){ // Не найден нужный блок, придется выделять
    str_syswrite(" using sbrk(), ");
    mem_to_return = seg_end; // Находимся в конце списка, он и будет возвращаемым адресом
    sbrk(allocsize); 
    seg_end = sbrk(0); // "хвост" сместился
    current_inf = mem_to_return;
    current_inf->size = allocsize; // Размер свежевыделенного куска

    ptr_syswrite(sbrk(0));
    str_syswrite("-tail ");
  }

  if (errno){ // Если выделение провалилось, sbrk() изменит errno.
  // Надеюсь, что его меняет тут только sbrk()
    str_syswrite("\n\n ----sbrk error ");
    int_syswrite(allocsize);
    str_syswrite("-size ");
    ptr_syswrite(seg_start);
    str_syswrite("-start ");
    ptr_syswrite(seg_end);
    str_syswrite("-tail ");
    ptr_syswrite(sbrk(0));
    str_syswrite("-sbrk(0) ");
    int_syswrite(errno);
    str_syswrite("-errno");
    str_syswrite(" sbrk error----\n\n");
    mem_to_return = NULL;
  }
  else{ // Если все нормально
    current_inf = mem_to_return;
    current_inf->avaliable = 0; // Блок больше не доступен
    mem_to_return = mem_to_return + inf_offset; // Чтобы не попортить блок информации
  }
    ptr_syswrite(mem_to_return);
    str_syswrite("-returned\n"); 
  return mem_to_return;
}

void* calloc(size_t length, size_t size){
  str_syswrite(" calloc: ");
  void* ptr = malloc(length*size); // Пытаемся найти новый блок
  if (ptr){
    memset(ptr, 0, length*size); // потому что надо заполнять нулями ; !segfault
    // Структуру не затрет, так как указатель сдвигается при создании
  }
  return ptr;
}

void free(void* ptr){ // Объявил перед realloc, потому что free там используется

if (wasinit == 0) {init();}
  str_syswrite("free: ");
    int_syswrite(errno);
    str_syswrite("-errno_fstart ");
  str_syswrite("releasing ");
  if(ptr){
    struct block_inf *del;
    del = ptr - inf_offset;
    del->avaliable = 1;
    int_syswrite(del->size);
    str_syswrite(" bytes at ");
  }
  ptr_syswrite(ptr);
  str_syswrite(" ");
    int_syswrite(errno);
    str_syswrite("-errno_fend \n");
}

void* realloc(void* ptr, size_t size){
  str_syswrite("realloc: ");
  void* new_ptr = malloc(size); // Пытаемся найти новый блок
  if(ptr)
    if (new_ptr){
      memcpy(new_ptr+inf_offset, ptr, size); // Если блок найден, копируем данные по новому адресу
      free(ptr); // ... и освобождаем старый
    }
  ptr_syswrite(new_ptr);
  str_syswrite(" returned\n");
  return new_ptr; // Если блок не будет найден или выделен, вернется NULL
}

