#ifndef TONESYNTH_H
#define TONESYNTH_H

#include <QAudioFormat>
#include <QIODevice>
#include <QMap> // Ассоциативный контейнер (словарь)
#include <QObject>
#include <QString>

class ToneSynthesizer : public QIODevice
{
    Q_OBJECT // Макрос, необходимый для использования механизма сигналов и слотов Qt

public:
    // Состояния огибающей
    enum class EnvelopeState : int { silentState, attackState, sustainState, releaseState };
    Q_ENUM(EnvelopeState)

    // Конструктор класса
    ToneSynthesizer(const QAudioFormat &format);
      // Переопределенные методы для чтения и записи данных, размера и доступного количества байт
    qint64 readData(char *data, qint64 maxlen) override;
    qint64 writeData(const char *data, qint64 len) override;
    qint64 size() const override;
    qint64 bytesAvailable() const override;

     // Методы для установки октавы, получения размера последнего буфера и сброса этого размера
    void setOctave(int newOctave);
    qint64 lastBufferSize() const;
    void resetLastBufferSize();

public slots:
    // Слоты для запуска, остановки, включения и выключения ноты
    void start();
    void stop();
    void noteOn(const QString &note);
    void noteOff();

private:
    QAudioFormat m_format;
    int m_octave; /* octave 3 */
    /* Equal temperament scale */
    // Словарь частот нот в герцах
    const QMap<QString, qreal> m_freq{{"C'", 261.626},
                                      {"B", 246.942},
                                      {"A#", 233.082},
                                      {"A", 220.000},
                                      {"G#", 207.652},
                                      {"G", 195.998},
                                      {"F#", 184.997},
                                      {"F", 174.614},
                                      {"E", 164.814},
                                      {"D#", 155.563},
                                      {"D", 146.832},
                                      {"C#", 138.591},
                                      {"C", 130.813}};
    qreal m_angleDelta;  // Приращение фаз для генерации синусоидального сигнала
    qreal m_currentAngle; // Текущая фаза
    qint64 m_lastBufferSize;
    EnvelopeState m_envelState; // Состояние огибающей
    qreal m_envelVolume; // Громкость огибающей
    qreal m_envelDelta; // Приращение громкости огибающей
    quint64 m_envelCount; // Счетчик сэмплов для огибающей
    quint64 m_attackTime; // Время атаки в сэмплах
    quint64 m_releaseTime; // Время затухания в сэмплах
};

#endif // TONESYNTH_H
