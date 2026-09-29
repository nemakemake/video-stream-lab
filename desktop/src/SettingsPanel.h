#pragma once

#include <QHash>
#include <QJsonObject>
#include <QWidget>

class QCheckBox;
class QFormLayout;
class QSlider;
class QSpinBox;

// Панель настроек камеры. Сама ничего не отправляет — сообщает сигналом
// parameterChanged, что пользователь изменил настройку.
class SettingsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPanel(QWidget *parent = nullptr);

    void setControlAvailable(bool available);
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
