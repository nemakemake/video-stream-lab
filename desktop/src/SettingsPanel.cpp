#include "SettingsPanel.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

SettingsPanel::SettingsPanel(QWidget *parent)
    : QWidget(parent)
    , m_content(new QWidget(this))
{
    auto *outer = new QVBoxLayout(this);
    outer->addWidget(m_content);
    outer->addStretch();

    auto *form = new QFormLayout(m_content);

    // Экспозиция и gain вручную работают только при выключенном авто-режиме
    // (прошивка сама выключает авто при ручной команде, но UI должен это отражать).
    auto *autoExposure = addCheckBox(form, QStringLiteral("Автоэкспозиция"),
                                     QStringLiteral("auto_exposure"), true);
    m_exposure = addSliderRow(form, QStringLiteral("Экспозиция"),
                              QStringLiteral("exposure"), 0, 1200, 300);

    auto *autoGain = addCheckBox(form, QStringLiteral("Автоусиление"),
                                 QStringLiteral("auto_gain"), true);
    m_gain = addSliderRow(form, QStringLiteral("Усиление (gain)"),
                          QStringLiteral("gain"), 0, 30, 0);

    addCheckBox(form, QStringLiteral("Автобаланс белого"), QStringLiteral("whitebal"), true);

    // 0..63, меньше = лучше качество и больше размер кадра.
    addSliderRow(form, QStringLiteral("Качество JPEG"),
                 QStringLiteral("jpeg_quality"), 4, 63, 12);

    auto syncManualEnabled = [](QCheckBox *autoBox, SliderRow row) {
        const bool manual = !autoBox->isChecked();
        row.slider->setEnabled(manual);
        row.spin->setEnabled(manual);
    };
    connect(autoExposure, &QCheckBox::toggled, this,
            [=] { syncManualEnabled(autoExposure, m_exposure); });
    connect(autoGain, &QCheckBox::toggled, this,
            [=] { syncManualEnabled(autoGain, m_gain); });
    syncManualEnabled(autoExposure, m_exposure);
    syncManualEnabled(autoGain, m_gain);

    setControlAvailable(false);
}

void SettingsPanel::setControlAvailable(bool available)
{
    m_content->setEnabled(available);
}

QCheckBox *SettingsPanel::addCheckBox(QFormLayout *form, const QString &label,
                                      const QString &param, bool initial)
{
    auto *box = new QCheckBox(label, m_content);
    box->setChecked(initial);
    form->addRow(box);
    connect(box, &QCheckBox::toggled, this, [this, param](bool on) {
        if (!m_applyingSettings)
            emit parameterChanged(param, on ? 1 : 0);
    });
    m_checkBoxes.insert(param, box);
    return box;
}

SettingsPanel::SliderRow SettingsPanel::addSliderRow(QFormLayout *form, const QString &label,
                                                     const QString &param, int min, int max,
                                                     int initial)
{
    auto *slider = new QSlider(Qt::Horizontal, m_content);
    slider->setRange(min, max);
    slider->setValue(initial);

    auto *spin = new QSpinBox(m_content);
    spin->setRange(min, max);
    spin->setValue(initial);

    // Слайдер и спинбокс показывают одно значение (QSignalBlocker не нужен:
    // setValue с тем же значением сигнал не испускает, цикла нет).
    connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
    connect(spin, &QSpinBox::valueChanged, slider, &QSlider::setValue);

    // Debounce: при перетаскивании отправляем только последнее значение.
    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(150);
    connect(slider, &QSlider::valueChanged, timer, qOverload<>(&QTimer::start));
    connect(timer, &QTimer::timeout, this,
            [this, slider, param] { emit parameterChanged(param, slider->value()); });

    auto *row = new QHBoxLayout();
    row->addWidget(slider, 1);
    row->addWidget(spin);
    form->addRow(label, row);

    m_sliderRows.insert(param, {slider, spin});
    return {slider, spin};
}

void SettingsPanel::applySettings(const QJsonObject &settings)
{
    // Флаг гасит команды от чекбоксов; у слайдеров блокируем сигналы, иначе
    // debounce-таймер отправил бы полученное значение обратно на устройство.
    m_applyingSettings = true;

    for (auto it = m_checkBoxes.cbegin(); it != m_checkBoxes.cend(); ++it) {
        if (settings.contains(it.key()))
            it.value()->setChecked(settings.value(it.key()).toInt() != 0);
    }

    for (auto it = m_sliderRows.cbegin(); it != m_sliderRows.cend(); ++it) {
        if (!settings.contains(it.key()))
            continue;
        const int value = settings.value(it.key()).toInt();
        const QSignalBlocker sliderBlocker(it.value().slider);
        const QSignalBlocker spinBlocker(it.value().spin);
        it.value().slider->setValue(value);
        it.value().spin->setValue(value);
    }

    m_applyingSettings = false;
}
