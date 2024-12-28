#include <QApplication>
#include "mainwindow.h"  // Заголовочный файл главного окна


int main(int argc, char *argv[])
{
    // Создаем объект приложения
    QApplication a(argc, argv);
     // Создаем объект главного окна
    MainWindow w;
    // Отображаем главное окно
    w.show();
     // Запускаем цикл обработки событий приложения
    return a.exec();
}
