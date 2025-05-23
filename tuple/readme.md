# Tuple

В этой задаче вам предлагается поупражняться в использовании variadic templates, а также type_traits с использованием SFINAE и/или концептов.

Напишите шаблонный класс с переменным количеством аргументов - `Tuple` (кортеж), обобщение класса `std::pair`, простенький аналог `std::tuple` из С++11.
Класс должен обладать следующей функциональностью:
- Конструктор по умолчанию (все элементы кортежа инициализируются значениями по умолчанию). Этот конструктор должен участвовать в перегрузке тогда и только тогда, когда `std::is_default_constructible<Ti>::value` верно для всех `i`. Этот конструктор должен быть explicit в том и только том случае, когда `Ti` не является `copy-list-initializable` из $\{\}$ для какого-либо `i`.
- Конструктор из набора аргументов соответствующих типов для Tuple (конструктор $(2)$ со страницы [tuple](https://en.cppreference.com/w/cpp/utility/tuple/tuple)). Этот конструктор должен участвовать в перегрузке, только если `std::is_copy_constructible<Ti>::value` верно для всех `i`. Этот конструктор должен быть `explicit`, если `std::is_convertible<const Ti&, Ti>::value` неверно хотя бы для одного `i`.
- Конструктор от универсального набора аргументов `UTypes&&... args` (конструктор $(3)$ со страницы [tuple](https://en.cppreference.com/w/cpp/utility/tuple/tuple)). Этот конструктор должен участвовать в перегрузке, только если размеры пакетов аргументов совпадают, эти размеры больше единицы, и `std::is_constructible<Ti, Ui>::value` верно для всех `i`. Этот конструктор должен быть `explicit`, если `std::is_convertible<Ti, Ui>::value` неверно хотя бы для одного `i`.
- Конструкторы от другого `Tuple<UTypes> other`, причем нужно поддержать версию $(5)$ от `const Tuple<UTypes>&` и версию $(6)$ от `Tuple<UTypes>&&`. Эти конструкторы должны инициализировать каждый элемент Tuple выражением `get<i>(FWD(other))`, где `FWD(other)` означает `std::forward<decltype(other)>(other)`.

Эти конструкторы должны участвовать в перегрузке, только если выполнены все условия:
- Размеры пакетов `Types` и `UTypes` совпадают;
- `std::is_constructible_v<Ti, decltype(get<i>(FWD(other)))>` верно для всех `i`;
- Или размер пакета `Types` не равен `1`, или `std::is_convertible_v<decltype(other), T>, std::is_constructible_v<T, decltype(other)>` и `std::is_same_v<T, U>` все имеют значение `false`.

Эти конструкторы должны быть `explicit`, если `std::is_convertible_v<decltype(std::get<i>(FWD(other))), Ti>` неверно хотя бы для одного `i`.

- Конструкторы от `std::pair<T1, T2>`, которые создают `Tuple<T1, T2>`. Нужно поддержать версию от `const std::pair<T1, T2>&` и от `std::pair<T1, T2>&&`. Для корректной работы этих конструкторов, скорее всего, понадобится написать template deduction guides.
- Конструкторы копирования и перемещения `Tuple`. Конструктор копирования должен кидать CE, если `std::is_copy_constructible<Ti>::value` неверно для какого-либо `i`. Конструктор перемещения должен не участвовать в перегрузке, если `std::is_move_constructible<Ti>::value` неверно для какого-либо `i`.
- Оператор копирующего присваивания `Tuple`. Если `std::is_copy_assignable<Ti>::value` неверно хотя бы для одного `i`, то его вызов должен приводить к CE.
- Оператор перемещающего присваивания. Должен присвоить каждому элементу `Tuple` выражение `std::forward<Ti>(get<i>(other))`. Должен участвовать в перегрузке, только если `std::is_move_assignable<Ti>::value` верно для всех `i`.
- Оператор присваивания от `const tuple<UTypes...>& other`. Должен участвовать в перегрузке, только если размеры пакетов совпадают и `std::is_assignable<Ti&, const Ui&>::value` верно для всех `i`.
- Оператор присваивания от `tuple<UTypes...>&& other`. Должен участвовать в перегрузке, только если размеры пакетов совпадают и `std::is_assignable<Ti&, Ui>::value` верно для всех `i`.
- Операторы присваивания от пары `const std::pair<T1, T2>&` и от `std::pair<T1, T2>&&`.

Конструкторы $(4)$, $(7)$, $(8)$, $(11)$ и $(12)$, а также $(15)-(28)$ со страницы [tuple](https://en.cppreference.com/w/cpp/utility/tuple/tuple) поддерживать необязательно. Также необязательно поддерживать операторы присваивания $(2)$, $(4)$, $(6)$, $(8)$, $(10)$, $(12)-(14)$ со страницы [operator=](https://en.cppreference.com/w/cpp/utility/tuple/operator%3D).

Помимо этого, нужно реализовать функции, не являющиеся членами класса:
- Функции `makeTuple`, `tie` и `forwardAsTuple`, создающие новый `Tuple` аналогично тому, как это делают функции `std::make_tuple`, `std::tie` и `std::forward_as_tuple`.
- Функция `get` с шаблонным параметром `size_t Index`, которая принимает `Tuple` и возвращает ссылку на `Index`-й элемент кортежа (считая, разумеется, от нуля). Причем ссылка должна быть того же вида, что была и ссылка на принятый `Tuple`.
- Функция `get` с шаблонным параметром `T`, которая принимает `Tuple` и возвращает ссылку на тот элемент кортежа, который имеет тип `T`. Причем ссылка должна быть того же вида, что была и ссылка на принятый `Tuple` (lvalue, const lvalue или rvalue). Если в `Tuple` нет элемента типа `T` или таких элементов несколько, то вызов `get<T>` должен приводить к ошибке компиляции.
- Функция `tupleCat`, которая возвращает кортеж, являющийся конкатенацией нескольких кортежей, переданных в качестве аргументов. При этом, если исходный кортеж был lvalue, то его элементы надо скопировать, а если rvalue -- то мувнуть в новоиспеченный кортеж. За время работы `tuple_cat` элементы исходных кортежей должны быть скопированы или мувнуты не более одного раза!
- Лексикографическое сравнение `Tuple`.