#pragma once

#include <QHash>
#include <QJsonObject>
#include <QWidget>

class QCheckBox;
class QFormLayout;
class QSlider;
class QSpinBox;

// Панель настроек камеры. Сама ничего не отправляет — только сообщает сигналом
// parameterChanged(param, value), что пользователь изменил настройку. Имена
// параметров совпадают с теми, что понимает прошивка (см. handleControlLine).
class SettingsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPanel(QWidget *parent = nullptr);

    // Панель доступна только при живом control-соединении.
    void setControlAvailable(bool available);

    // Показывает настройки, полученные от устройства. Ответных команд не шлёт.
    void applySettings(const QJsonObject &settings);

signals:
    void parameterChanged(const QString &param, int value);

private:
    struct SliderRow {
        QSlider *slider;
        QSpinBox *spin;
    };

    SliderRow addSliderRow(QFormLayout *form, const QString &label,
                           const QString &param, int min, int max, int initial);
    QCheckBox *addCheckBox(QFormLayout *form, const QString &label,
                           const QString &param, bool initial);

    QWidget *m_content;
    SliderRow m_exposure{};
    SliderRow m_gain{};

    QHash<QString, QCheckBox *> m_checkBoxes;
    QHash<QString, SliderRow> m_sliderRows;
    bool m_applyingSettings = false;
};
