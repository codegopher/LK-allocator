#include <unistd.h>
#include <sys/syscall.h>
#include <string.h> // memset, memcpy
#include <errno.h>

// gcc -fpic -shared -g -pedantic allocator.c -o alloc && gcc test.c  -g -o run && LD_PRELOAD=./alloc ps - тест этой библиотеки
// В корне лежит также тестовый файл с вызовом malloc и free

// Все переопределяемые функции изначально определены в stdlib.h

void *first_block;
void *last_block;
int wasinit = 0; // флаг инициализированности. Иначе просто не вышло

// Как вызывать функцию init? - Никак. Просто явно задать глобальные переменные.
// Сделать список двусвязным
// Дописал возврат NULL, если выделение не прошло успешно

// Будем писать информацию о блоке непосредственно перед ним
struct block_inf{
  char avaliable;
  size_t size;
};
int inf_offset = sizeof(struct block_inf); // Размер блока, чтобы постоянно не дергать sizeof

int main(){
  int r;
  r = syscall(SYS_write,1,"hw\n",3); // номер вызова, stdout, строка, длина
  return 0;
}

void init(){
  syscall(SYS_write,1,"init\n",5); // отладочное сообщение
  wasinit = 1;
  last_block = sbrk(0); // Последний доступный - в хвосте процесса
  first_block = last_block;
}

void* malloc(size_t size){
  if (wasinit == 0) {init();} // увы, пришлось так
  syscall(SYS_write,1,"malloc\n",7);

  size = size + inf_offset;
  int allocsize = size &= ~3; // Кратный 4 с округлением в большую сторону объем памяти
  void* current_pos = first_block; // Текущее "местонахождение"
  void* mem_to_return; // Что вернем
  char found = 0; // Подходящий блок найден
  struct block_inf *current_inf; // Информация о текущем блоке

  while ( (current_pos != last_block) && (found == 0)){
    current_inf = current_pos; // Информация о текущем блоке
    if ( (current_inf->size >= allocsize) &&  (current_inf->avaliable == 1)) {
    // Если блок свободен и подходит по размеру, используем его
      found = 1;
      mem_to_return = current_pos;
    }
    current_pos = current_pos + current_inf->size;
  }

  if (found == 0){ // Не найден нужный блок, придется выделять
    mem_to_return = sbrk(allocsize);
  }

  if (errno){ // Если выделение провалилось, sbrk() изменит errno.
  // Надеюсь, что его меняет тут только sbrk()
    mem_to_return = NULL; // Вернем NULL
  }
  else{ // Если все нормально
    last_block = mem_to_return;
    current_inf = mem_to_return;
    current_inf->avaliable = 0; // Блок больше не доступен
    mem_to_return = mem_to_return + inf_offset; // Чтобы не попортить блок информации
  
  }
 
  return mem_to_return;
}

void* calloc(size_t length, size_t size){
  if (wasinit == 0) {init();}
  syscall(SYS_write,1,"calloc\n",7);
  void* ptr = malloc(length*size); // Пытаемся найти новый блок
  if (ptr){
    memset(ptr, 0, length*size); // потому что надо заполнять нулями ; !segfault
    // Структуру не затрет, так как указатель сдвигается при создании
  }
  return ptr;
}

void free(void* ptr){ // Объявил перед realloc, потому что free там используется
if (wasinit == 0) {init();}
  syscall(SYS_write,1,"free\n",5);
  struct block_inf *del;
  del = ptr - inf_offset;
  syscall(SYS_write,1,"-avl\n",5);
  del->avaliable = 1; // !segfault
  syscall(SYS_write,1,"-suc\n",5);
}

void* realloc(void* ptr, size_t size){
if (wasinit == 0) {init();}
syscall(SYS_write,1,"realloc\n",8);
  void* new_ptr = malloc(size); // Пытаемся найти новый блок
  if (new_ptr){
    memcpy(new_ptr+inf_offset, ptr, size); // Если блок найден, копируем данные по новому адресу
    free(ptr); // ... и освобождаем старый
  }
  return new_ptr; // Если блок не будет найден или выделен, вернется NULL
}

