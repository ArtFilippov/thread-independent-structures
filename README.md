# thread_independent_structures
header only библиотека, содержащая реализацию различных структур данных для многопоточных программ. В основу библиотеки легла книга Энтони Уильямса "C++ Concurrency in Action"
## Интеграция
Клонируйте репозиторий, подключайте заголовки
## Описание и примеры
### safe_queue/threadsafe_queue.h
Содержит шаблон класса `threadsafe_queue`. Параметр шаблона - тип хранимых элементов
```cpp
#include <cassert>
#include <thread_independent_structures/safe_queue/threadsafe_queue.h>
 
int main()
{
    std::threadsafe_queue<int> q;
 
    q.push(0); // back pushes 0
    q.push(1); // q = 0 1
    q.push(2); // q = 0 1 2
    q.push(3); // q = 0 1 2 3
 
 
    if (auto front = q.try_pop()) {
        assert(*front == 0);
    } else {
        assert(q.empty() == true);
    }
}
```
### thread_pool/join_threads.h
Содержит класс `join_threads`, реализующий идиому RAII для управления присоединением потоков. Объект класса создаёт пустой вектор потоков, его методы можно вызывать через `operator->()`. В деструкторе класса присоединяются все потоки из вектора
```cpp
#include <iostream>
#include <thread_independent_structures/thread_pool/join_threads.h>
 
int main()
{
    join_threads threads;
    for (int i = 0; i < 10; ++i) {
        auto new_thread = std::thread([=](){ std::cout << i << std::endl});
        threads->push_back(std::move(new_thread));
    }
} // при выходе из программы, все потоки завершатся
```
### thread_pool/fine_grained_thread_pool.h
Содержит класс `fine_grained_thread_pool`, реализующий пул потоков. Конструктор принимает один параметр - количество потоков в пуле. Значение по умолчанию - hardware_concurrency(). Примеры использования смотрите [здесь](https://gitea/filippar/thread_independent_structures/src/branch/main/tests/thread_pool/test_fine_grained_thread_pool.h)

### thread_pool/shared_result.h
Содержит шаблоны классов `shared_result` и `shared_result_control_block` для ожидания завершения асинхронных задач. Параметр шаблонов - тип ожидаемого значения 

- `shared_result_control_block`
    - Реализует медоты для контроля состояния задачи, помещённой в пул потоков, или созданой с помощью `std::async()`, или с помощью какой-либо связки future/promise. 

- `shared_result`
    - Является надстройкой над `std::shared_future` и позволяет делать всё, что можно делать с `std::shared_future`
    - Получить экземпляры `shared_result` можно только с помощью вызова `shared_result_control_block.share()`, или `shared_result_control_block.new_share()`, или через конструктор копирования
    - После разрушения всех `std::shared_future`, связанных с одним `shared_result_control_block`, вызов соответсвующего `shared_result_control_block.does_it_expect()` вернёт `false`. Это можно использовать в условии досрочного завершения задачи в `fine_grained_thread_pool`

Пример использования смотрите [здесь](https://gitea/filippar/thread_independent_structures/src/branch/main/tests/thread_pool/test_shared_result.h)
