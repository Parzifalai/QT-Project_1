#include <limits> // Для получения максимального значения qint64
//#include <QDebug>
#include <QtMath>
#include "tonesynth.h"

ToneSynthesizer::ToneSynthesizer(const QAudioFormat &format)
    : QIODevice()
    , m_octave(3) // Начальная октава 3
    , m_envelState(EnvelopeState::silentState) // Изначально состояние огибающей - "тишина"
    , m_envelVolume(0.0) // Начальная громкость огибающей 0.0
{
    //qDebug() << Q_FUNC_INFO;
    if (format.isValid()) {
        m_format = format;
        m_attackTime = (quint64) (0.02 * format.sampleRate()); // Время атаки 20 мс
        m_releaseTime = m_attackTime; // Время затухания равно времени атаки
        m_envelDelta = 1.0 / m_attackTime; // Шаг изменения громкости
    }
}

// Запуск синтезатора
void ToneSynthesizer::start()
{
    //qDebug() << Q_FUNC_INFO;
    // Открываем устройство в режиме только для чтения и без буферизации
    open(QIODevice::ReadOnly | QIODevice::Unbuffered);
}

// Остановка синтезатора
void ToneSynthesizer::stop()
{
    //qDebug() << Q_FUNC_INFO;
    if (isOpen()) {
        close();
    }
}

// Включение ноты
void ToneSynthesizer::noteOn(const QString &note)
{
    // Если нота есть в словаре частот
    if (m_freq.contains(note)) {
        // Вычисляем частоту ноты с учетом октавы
        qreal noteFreq = qPow(2, m_octave - 3) * m_freq[note];
        // Вычисляем приращение фазы за сэмпл
        qreal cyclesPerSample = noteFreq / m_format.sampleRate();
        m_angleDelta = cyclesPerSample * 2.0 * M_PI;
        m_currentAngle = 0.0;  // Сбрасываем текущую фазу

        m_envelState = EnvelopeState::attackState; // Переходим в состояние атаки
        m_envelCount = m_attackTime; // Устанавливаем счетчик сэмплов атаки
        m_envelVolume = 0.0; // Начальная громкость 0
    }
}

// Выключение ноты
void ToneSynthesizer::noteOff()
{
    //    qDebug() << Q_FUNC_INFO
    //             << "last synth period:"
    //             << m_lastBufferSize << "bytes,"
    //             << m_format.durationForBytes(m_lastBufferSize) / 1000
    //             << "milliseconds";
    m_envelState = EnvelopeState::releaseState; // Переходим в состояние затухания
    m_envelCount = m_releaseTime;
}

// Получение размера последнего сгенерированного буфера
qint64 ToneSynthesizer::lastBufferSize() const
{
    return m_lastBufferSize;
}

// Сброс размера последнего сгенерированного буфера
void ToneSynthesizer::resetLastBufferSize()
{
    m_lastBufferSize = 0;
}

// Установка октавы
void ToneSynthesizer::setOctave(int newOctave)
{
    m_octave = newOctave;
}

// Чтение данных из устройства (основной метод генерации звука)
qint64 ToneSynthesizer::readData(char *data, qint64 maxlen)
{
    //qDebug() << Q_FUNC_INFO << maxlen;
    const qint64 channelBytes =
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        m_format.sampleSize() / CHAR_BIT;
#else
        m_format.bytesPerSample();
#endif
    Q_ASSERT(channelBytes > 0);
    // Выравниваем длину по размеру сэмпла
    qint64 length = (maxlen / channelBytes) * channelBytes;
    qint64 buflen = length;
    // Приводим указатель на данные к типу unsigned char*
    unsigned char *ptr = reinterpret_cast<unsigned char *>(data);

    // Цикл по сэмплам
    while (length > 0) { // Текущий сэмпл
        // Генерация огибающей
        float currentSample = 0.0;
         // Генерация огибающей
        switch (m_envelState) {
        case EnvelopeState::silentState:
            break;
        case EnvelopeState::attackState:
            if (m_envelCount > 0) {
                m_envelVolume += m_envelDelta;
                m_envelCount--;
            } else {
                m_envelVolume = 1.0;
                m_envelState = EnvelopeState::sustainState;
            }
            break;
        case EnvelopeState::sustainState:
            break;
        case EnvelopeState::releaseState:
            if (m_envelCount > 0) {
                m_envelVolume -= m_envelDelta;
                m_envelCount--;
            } else {
                m_envelVolume = 0.0;
                m_envelState = EnvelopeState::silentState;
            }
            break;
        }

        // Генерация синусоидального сигнала
        if (m_envelState != EnvelopeState::silentState) {
            currentSample = m_envelVolume * qSin(m_currentAngle); // Генерируем сэмпл синусоиды с текущей фазой и громкостью
            m_currentAngle += m_angleDelta; // Увеличиваем фазу на приращение
        }
        // Запись сэмпла в буфер(float)
        *reinterpret_cast<float *>(ptr) = currentSample;
        ptr += channelBytes; // Перемещаем указатель на следующий сэмпл
        length -= channelBytes; // Уменьшаем оставшуюся длину
    }
    m_lastBufferSize = buflen;
    return buflen;
}

// Запись данных в устройство
qint64 ToneSynthesizer::writeData(const char *data, qint64 len)
{
    Q_UNUSED(data);
    Q_UNUSED(len);
    return 0;
}

// Возвращение количества доступных байт
qint64 ToneSynthesizer::size() const
{
    //qDebug() << Q_FUNC_INFO;
    return std::numeric_limits<qint64>::max();
}

qint64 ToneSynthesizer::bytesAvailable() const
{
    //qDebug() << Q_FUNC_INFO;
    return std::numeric_limits<qint64>::max();
}
