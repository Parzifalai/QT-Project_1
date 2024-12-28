#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QByteArray> // Для работы с байтовыми массивами
#include <QComboBox> // Выпадающий список
#include <QIODevice> // Базовый класс для устройств ввода/вывода
#include <QLabel>  // Текстовая метка
#include <QMainWindow> // Главное окно приложения
#include <QMap> // Ассоциативный контейнер (словарь)
#include <QObject> // Базовый класс для всех объектов Qt
#include <QPushButton> // Кнопка
#include <QScopedPointer> // Умный указатель с автоматическим удалением объекта
#include <QSlider> // Ползунок
#include <QString> // Строка
#include <QTimer> // Таймер
#include <QtMath> // Математические функции

#include <QAudioFormat>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QAudioOutput>
#else
#include <QAudioSink>
#endif

#include "tonesynth.h" // Заголовочный файл синтезатора

QT_BEGIN_NAMESPACE // Открываем пространство имен Qt
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE // Закрываем пространство имен Qt

// Класс главного окна приложения
class MainWindow : public QMainWindow
{
    Q_OBJECT // Макрос, необходимый для использования механизма сигналов и слотов Qt

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void underrunDetected(); // Сигнал о нехватке данных в аудио-буфере
    void stallDetected(); // Сигнал о "зависании" аудиовывода

private:
    void initializeWindow();
    void initializeAudio();

private slots:
    void deviceChanged(int index);
    void volumeChanged(int value);
    void bufferChanged(int value);
    void octaveChanged(int value);
#if !defined(Q_OS_WASM)
    // Слоты для вывода сообщений об ошибках (только для не-WebAssembly платформ)
    void underrunMessage();
    void stallMessage();
#endif
     // Переопределенные методы обработки событий клавиатуры
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    Ui::MainWindow *m_ui;
    QAudioFormat m_format;
    int m_bufferTime;
    bool m_running;
    QScopedPointer<ToneSynthesizer> m_synth; // Умный указатель на объект синтезатора тона
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QScopedPointer<QAudioOutput> m_audioOutput;
#else
    QScopedPointer<QAudioSink> m_audioOutput;
#endif
#if !defined(Q_OS_WASM)
    QTimer m_stallDetector; // Таймер для определения "зависания" аудио(не для WebAssembly)
#endif
};

#endif // MAINWINDOW_H
