//#include <QDebug> (Для отладочного вывода)
#include <QtMath>
#if !defined(Q_OS_WASM)
#include <QMessageBox> // Для отображения диалоговых окон
#endif
#include <QTimer>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
// Для Qt5
#include <QAudioDeviceInfo>
#include <QAudioOutput>
#else
// Для Qt6
#include <QAudioDevice>
#include <QAudioSink>
#include <QMediaDevices>
#endif

#include "mainwindow.h" // Заголовочный файл главного окна
#include "ui_mainwindow.h" // Сгенерированный заголовочный файл из .ui файла
#include <QKeyEvent>  // Событие клавиатуры

// Конструктор класса MainWindow
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_ui(new Ui::MainWindow) // Инициализируем указатель на объект UI
#if defined(Q_OS_WASM)
    , m_bufferTime(150) // Размер буфера для WebAssembly (150 мс)
#else
    , m_bufferTime(100) // Размер буфера в мс
#endif
    , m_running(false) // Изначально аудио не запущено
{
    //qDebug() << Q_FUNC_INFO;
    m_ui->setupUi(this); // Настраиваем пользовательский интерфейс
    initializeWindow(); // Инициализируем окно
    initializeAudio(); // Инициализируем аудио
}

MainWindow::~MainWindow()
{
    //qDebug() << Q_FUNC_INFO;
#if !defined(Q_OS_WASM)
    m_stallDetector.stop(); // Останавливаем таймер stallDetector
#endif
    m_audioOutput->stop();
    if (!m_synth.isNull()) {
        m_synth->stop();
    }
    delete m_ui;
}

void MainWindow::initializeWindow()
{
    //qDebug() << Q_FUNC_INFO;
    m_format.setSampleRate(44100); // Частота дискретизации
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_format.setChannelCount(1); // Моно
    m_format.setSampleSize(sizeof(float) * CHAR_BIT); // Устанавливаем размер сэмпла (float = 32 бита)
    m_format.setCodec("audio/pcm"); // Устанавливаем кодек PCM
    m_format.setByteOrder(QAudioFormat::LittleEndian);  // Устанавливаем порядок байт LittleEndian
    m_format.setSampleType(QAudioFormat::Float); // Устанавливаем тип сэмпла Float
    const QAudioDeviceInfo &defaultDeviceInfo = QAudioDeviceInfo::defaultOutputDevice();
    m_ui->deviceBox->addItem(defaultDeviceInfo.deviceName(), QVariant::fromValue(defaultDeviceInfo));
    for (auto &deviceInfo : QAudioDeviceInfo::availableDevices(QAudio::AudioOutput)) {
        if (deviceInfo != defaultDeviceInfo && deviceInfo.isFormatSupported(m_format))
            m_ui->deviceBox->addItem(deviceInfo.deviceName(), QVariant::fromValue(deviceInfo));
    }
    m_ui->deviceBox->setCurrentText(defaultDeviceInfo.deviceName());
#else
    m_format.setChannelConfig(QAudioFormat::ChannelConfigMono);  // Моно
    m_format.setSampleFormat(QAudioFormat::Float); // Формат сэмпла - float
    const QAudioDevice &defaultDeviceInfo = QMediaDevices::defaultAudioOutput();
    m_ui->deviceBox->addItem(defaultDeviceInfo.description(),
                             QVariant::fromValue(defaultDeviceInfo));
    for (auto &deviceInfo : QMediaDevices::audioOutputs()) {
        if (deviceInfo != defaultDeviceInfo && deviceInfo.isFormatSupported(m_format))
            m_ui->deviceBox->addItem(deviceInfo.description(), QVariant::fromValue(deviceInfo));
    }
    m_ui->deviceBox->setCurrentText(defaultDeviceInfo.description());
#endif
    m_synth.reset(new ToneSynthesizer(m_format));
    m_ui->bufferSpin->setValue(m_bufferTime);
    connect(m_ui->deviceBox, SIGNAL(activated(int)), this, SLOT(deviceChanged(int)));
    connect(m_ui->volumeSlider, SIGNAL(valueChanged(int)), this, SLOT(volumeChanged(int)));
    connect(m_ui->bufferSpin, SIGNAL(valueChanged(int)), this, SLOT(bufferChanged(int)));
    connect(m_ui->octaveSpin, SIGNAL(valueChanged(int)), this, SLOT(octaveChanged(int)));
#if !defined(Q_OS_WASM)
    connect(this, &MainWindow::underrunDetected, this, &MainWindow::underrunMessage);
    connect(this, &MainWindow::stallDetected, this, &MainWindow::stallMessage);
    connect(&m_stallDetector, &QTimer::timeout, this, [=] {
        if (m_running) {
            if (m_synth->lastBufferSize() == 0) {
                emit stallDetected();
            }
            m_synth->resetLastBufferSize();
        }
    });
#endif
     // Подключение кнопок к синтезатору
    auto buttons = findChildren<QPushButton *>();
    // Для каждой кнопки
    foreach (const auto btn, buttons) {
        connect(btn, &QPushButton::pressed, this, [=] { m_synth->noteOn(btn->text()); });
        // Соединяем сигнал pressed() (нажатие кнопки) с лямбда-функцией, которая вызывает noteOn() синтезатора
        connect(btn, &QPushButton::released, m_synth.get(), &ToneSynthesizer::noteOff);
    }
}

void MainWindow::initializeAudio()
{
    //qDebug() << Q_FUNC_INFO << m_ui->deviceBox->currentText();
    m_running = false; // Сбрасываем флаг запуска аудио
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Для Qt5: получаем объект QAudioDeviceInfo из текущего элемента комбобокса deviceBox
    const QAudioDeviceInfo deviceInfo = m_ui->deviceBox->currentData().value<QAudioDeviceInfo>();
#else
    // Для Qt6: получаем объект QAudioDevice из текущего элемента комбобокса deviceBox
    const QAudioDevice deviceInfo = m_ui->deviceBox->currentData().value<QAudioDevice>();
#endif
    // Проверяем, поддерживает ли устройство заданный формат
    if (!deviceInfo.isFormatSupported(m_format)) {
#if !defined(Q_OS_WASM)
        // Если не поддерживает, выводим сообщение об ошибке
        QMessageBox::warning(this,
                             "Audio format not supported",
                             "The selected audio device does not support the synth's audio format. "
                             "Please select another device.");
#endif
        return;
    }
    qint64 bufferLength = m_format.bytesForDuration(m_bufferTime * 1000); // Размер буфера в байтах
    //    qDebug() << "requested buffer size:" << bufferLength
    //             << "bytes," << m_bufferTime << "milliseconds";
    m_synth->start(); // Запускаем синтезатор
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Для Qt5: создаем объект QAudioOutput
    m_audioOutput.reset(new QAudioOutput(deviceInfo, m_format));
    QObject::connect(m_audioOutput.data(),
                     &QAudioOutput::stateChanged,
                     this,
                     [=](QAudio::State state) {
#else
    // Для Qt6: создаем объект QAudioSink
    m_audioOutput.reset(new QAudioSink(deviceInfo, m_format));
    QObject::connect(m_audioOutput.data(), &QAudioSink::stateChanged, this, [=](QAudio::State state) {
#endif \
    //qDebug() << "Audio Output state:" << state << "error:" << m_audioOutput->error();
                         if (m_running && (m_audioOutput->error() == QAudio::UnderrunError)) {
                             emit underrunDetected();
                         }
                     });
    m_audioOutput->setBufferSize(bufferLength);
    m_audioOutput->start(m_synth.get());
    auto bufferTime = m_format.durationForBytes(m_audioOutput->bufferSize()) / 1000; // Вычисляем реальный размер буфера в мс
    //    qDebug() << "applied buffer size:" << m_audioOutput->bufferSize()
    //             << "bytes," << bufferTime << "milliseconds";
    volumeChanged(m_ui->volumeSlider->value()); // Устанавливаем начальную громкость
    octaveChanged(m_ui->octaveSpin->value()); // Устанавливаем начальную октаву
#if !defined(Q_OS_WASM)
    // Запускаем таймер, который через bufferTime * 2 мс установит флаг m_running в true и запустит таймер m_stallDetector
    QTimer::singleShot(bufferTime * 2, this, [=] {
        m_running = true;
        m_stallDetector.start(bufferTime * 4);
    });
#endif
}

void MainWindow::deviceChanged(int index)
{
    //qDebug() << Q_FUNC_INFO << m_ui->deviceBox->itemText(index);
#if !defined(Q_OS_WASM)
    m_stallDetector.stop();
#endif
    m_audioOutput->stop();
    if (!m_synth.isNull()) {
        m_synth->stop();
    }
    initializeAudio();
}

void MainWindow::volumeChanged(int value)
{
    //qDebug() << Q_FUNC_INFO << value;
     // Преобразование логарифмической шкалы громкости в линейную
    qreal linearVolume = QAudio::convertVolume(value / 100.0,
                                               QAudio::LogarithmicVolumeScale,
                                               QAudio::LinearVolumeScale);
    m_audioOutput->setVolume(linearVolume);
}

void MainWindow::bufferChanged(int value)
{
    if (m_bufferTime != value) {
        m_bufferTime = value;
        deviceChanged(m_ui->deviceBox->currentIndex());
        //qDebug() << Q_FUNC_INFO << value;
    }
}

void MainWindow::octaveChanged(int value)
{
    //qDebug() << Q_FUNC_INFO << value;
    m_synth->setOctave(value);
}

#if !defined(Q_OS_WASM)
// Слот для вывода сообщения об ошибке Underrun
void MainWindow::underrunMessage()
{
    m_running = false;
    QMessageBox::warning(this,
                         "Underrun Error",
                         "Audio buffer underrun errors have been detected."
                         " Please increase the buffer time to avoid this problem.");
    m_running = true;
}

// Слот для вывода сообщения об ошибке Stall
void MainWindow::stallMessage()
{
    QMessageBox::critical(this,
                          "Audio Output Stalled",
                          "Audio output is stalled right now. Sound cannot be produced."
                          " Please increase the buffer time to avoid this problem.");
    m_running = false;
    m_stallDetector.stop();
}



#endif

// Обработчик нажатия клавиш
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    QString note;

    // Определяем, какая клавиша нажата
    switch (event->key()) {
    case Qt::Key_Q:
        note = "C";
        break;
    case Qt::Key_W:
        note = "D";
        break;
    case Qt::Key_E:
        note = "E";
        break;
    case Qt::Key_R:
        note = "F";
        break;
    case Qt::Key_T:
        note = "G";
        break;
    case Qt::Key_Y:
        note = "A";
        break;
    case Qt::Key_U:
        note = "B";
        break;
    case Qt::Key_I:
        note = "C'";
        break;
    case Qt::Key_2:
        note = "C#";
        break;
    case Qt::Key_3:
        note = "D#";
        break;
    case Qt::Key_5:
        note = "F#";
        break;
    case Qt::Key_6:
        note = "G#";
        break;
    case Qt::Key_7:
        note = "A#";
        break;
    default:
        QMainWindow::keyPressEvent(event);
        return;
    }
    // Если нажата клавиша, соответствующая ноте, отправляем сигнал
    if(!note.isEmpty())
    {
        m_synth->noteOn(note); // Включаем ноту в синтезаторе
    }
}

// Обработчик отпускания клавиш
void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    // Определяем, какая клавиша отпущена (для всех клавиш, которые могут включать ноты)
    switch (event->key()) {
    case Qt::Key_Q:
    case Qt::Key_W:
    case Qt::Key_E:
    case Qt::Key_R:
    case Qt::Key_T:
    case Qt::Key_Y:
    case Qt::Key_U:
    case Qt::Key_I:
    case Qt::Key_2:
    case Qt::Key_3:
    case Qt::Key_5:
    case Qt::Key_6:
    case Qt::Key_7:
        m_synth->noteOff(); // Отпускаем ноту при отпускании клавиши
    default:
        // Если отпущена другая клавиша, вызываем обработчик по умолчанию
        QMainWindow::keyReleaseEvent(event);
        return;
    }
}

