# Настройка окружения

Для решения задач вам потребуется компьютер с Unix окружением. Лучше всего использовать Linux.
OSX тоже будет нормально работать.

Можно использовать Windows, но для компиляции и запуска потребуется WSL (Windows Subsystem for Linux).
В этом случае сначала установите WSL, а дальшейшие инструкции выполняйте уже в Linux окружении.

Рекомендуется использовать Ubuntu версии **22.04**.
Тестирующая система проверяет задачи на этой
версии убунты, и если вы будете использовать эту же версию локально,
то вы избежите проблем со старыми версиями компиляторов.
Поддерживаемые компиляторы и способ их установки в Ubuntu:

- **g++-12**
```bash
$ sudo apt-get install g++-12
```
- **clang++-18**
```bash
$ wget https://apt.llvm.org/llvm.sh
$ chmod +x llvm.sh
$ sudo ./llvm.sh 18 all
```

При возникновении проблем с наличием библиотек (например, **libobjc**) для **clang++-18** следует выполнить
```bash
$ sudo apt install libclang-18-dev
```

## Регистрация в системе

1. Зарегистрируйтесь в системе [manytask]. Код регистрации доступен в чате курса.
Если вы ранее регистрировались в этой системе для прохождения других курсов, то сразу можете жать кнопку **Login**.
2. Сгенерируйте ssh ключ, если у вас его еще нет.
   ```bash
   $ ssh-keygen -N "" -f ~/.ssh/id_rsa
   ```
3. Скопируйте содержимое файла `id_rsa.pub` (`cat ~/.ssh/id_rsa.pub`) в https://gitlab.manytask.org/-/profile/keys
4. Проверьте, что ssh ключ работает. Выполните команду `ssh git@gitlab.manytask.org`. Вы должны увидеть такое приветствие:
   ```bash
   $ ssh git@gitlab.manytask.org
   PTY allocation request failed on channel 0
   Welcome to GitLab, USERNAME!
   Connection to gitlab.manytask.org closed.
   ```

## Настройка локального репозитория

1. На странице [manytask] нажмите на **MY REPO**
2. Нажмите на синию кнопку **Code** и скопируйте SSH адрес
3. Используя консоль склонируйте и настройте репозиторий с задачами:
```bash
$ git clone "<Полученный SSH адрес>"

# Переходим в директорию склонированного репозитория
$ cd "<Склонированный репозиторий>"

# Настраиваем пользователя
$ git config --local user.name "<логин с manytask>"
$ git config --local user.email "<email с manytask>"

# Настраиваем возможность получать обновления из публичного репозитория с задачами
$ git remote add upstream git@gitlab.manytask.org:mipt-cpp/public/public-2025-spring.git
```

В течении всего курса публичный репозиторий будет обновляться, в нём будут появляться новые задачи,
а также возможно будут обновляться тесты и условия существующих задач.
Подтянуть изменения из публичного рупозитория можно с помощью команды `git pull upstream main`

## Посылка тестовой задачи в систему

1. Настройте IDE. Если вы используйте Windows, то вам также надо настроить WSL:
   * CLion
      - \[Windows\] [Настройка WSL](https://www.jetbrains.com/help/clion/how-to-use-wsl-development-environment-in-clion.html)
      - Настройку можно посмотреть в [записи семинара из ШАДа](https://disk.yandex.ru/i/8waWBV-L-FOKOw)
   * VS Code
      - \[Windows\] [Настройка WSL](https://code.visualstudio.com/docs/cpp/config-wsl)
      - Настройка описана в [инструкции](https://docs.google.com/document/d/1mb4oGIJcU1vkNHoW0lHV_MsiolrWoLdG)

2. Решите первую задачу [multiplication](../multiplication).

3. Сдайте тестовое задание в систему:
```bash
# Находясь в корне репозитория
$ git checkout main
$ git checkout -b submit/multiplication
$ git add multiplication/multiplication.h
$ git commit -m "Add multiplication task"
$ git push origin main
```
*Примечание:* для сдачи других заданий необходимо называть ветку задачи в соответствии с её названием. Например, для задачи `my_task` нужно изменить вторую команду следующим образом:
```bash
$ git checkout -b "submit/my_task"
```

4. Пронаблюдайте за процессом тестирования на странице CI/CD -> Pipelines своего репозитория. gitlab показывает вывод консоли во время тестирования.

5. Проверьте, что оценка появилась в [таблице с результатами]

## Установка и запуск линтера

```bash
$ wget https://apt.llvm.org/llvm.sh
$ chmod +x llvm.sh
$ sudo ./llvm.sh 18 all
```

Дополнительные действия **только на Ubuntu**
```bash
$ sudo ln -s /usr/bin/clang-format-18 /usr/bin/clang-format 
$ sudo ln -s /usr/bin/clang-tidy-18 /usr/bin/clang-tidy
```

```bash
# Находясь в build директории
../run_linter.sh <имя задачи>
```
