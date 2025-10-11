#include "IntegratedValueController.h"
#include <QSlider>
#include <QDoubleSpinBox>
#include <QSignalBlocker>
#include <QtMath>
#include <algorithm>

IntegratedValueController::IntegratedValueController(QObject* parent,
                                                     QSlider* slider,
                                                     QDoubleSpinBox* spin,
                                                     double step)
    : QObject(parent), slider_(slider), spin_(spin)
{
    Q_ASSERT(slider_ && spin_);
    setSingleStep(step);   // initialize step & scale
    setRange(min_, max_);  // apply to widgets
    setValue(0.0);

    // connect widget signals
    connect(spin_,   qOverload<double>(&QDoubleSpinBox::valueChanged),
            this,    &IntegratedValueController::onSpinChanged);
    connect(spin_,   &QDoubleSpinBox::editingFinished,
            this,    &IntegratedValueController::onSpinEditingFinished);
    connect(slider_, &QSlider::valueChanged,
            this,    &IntegratedValueController::onSliderChanged);
    connect(slider_, &QSlider::sliderReleased,
            this,    &IntegratedValueController::onSliderReleased);
}

void IntegratedValueController::setRange(double min, double max)
{
    if (min > max) std::swap(min, max);
    min_ = min;
    max_ = max;

    if (spin_) {
        QSignalBlocker b(*spin_);
        spin_->setRange(min_, max_);
        spin_->setSingleStep(step_);
    }
    updateSliderRange_();

    setValue(std::clamp(value(), min_, max_));
}

void IntegratedValueController::setValue(double v)
{
    v = std::clamp(v, min_, max_);
    applyToChildren_(v, /*emitChange*/true);
}

double IntegratedValueController::value() const
{
    return spin_ ? spin_->value() : 0.0;
}

QByteArray IntegratedValueController::valueAsBytes()
{

    if (spin_) return int2Bytes((qint16)(spin_->value() * 10));

    return QByteArray();
}

void IntegratedValueController::setSingleStep(double step)
{
    step_  = std::max(step, 1e-6);
    scale_ = std::max(1, static_cast<int>(qRound(1.0 / step_))); // e.g., 0.1 → 10

    if (spin_) {
        QSignalBlocker b(*spin_);
        spin_->setSingleStep(step_);
    }
    updateSliderRange_();
}

void IntegratedValueController::setSliderPageStep(double step)
{
    if (!slider_) return;
    QSignalBlocker b(*slider_);
    slider_->setPageStep(std::max(1, static_cast<int>(qRound(step * scale_))));
}

void IntegratedValueController::setDecimals(int decimals)
{
    if (spin_) spin_->setDecimals(std::clamp(decimals, 0, 6));
}

void IntegratedValueController::onSpinChanged(double v)
{
    if (updating_) return;
    applyToChildren_(v, /*emitChange*/true);
}

void IntegratedValueController::onSpinEditingFinished()
{
    emit valueEditedByUser(value());
}

void IntegratedValueController::onSliderChanged(int iv)
{
    if (updating_) return;
    const double v = iv / static_cast<double>(scale_);
    applyToChildren_(v, /*emitChange*/true);
}

void IntegratedValueController::onSliderReleased()
{
    emit valueEditedByUser(value());
}

void IntegratedValueController::applyToChildren_(double v, bool emitChange)
{
    updating_ = true;

    if (spin_) {
        QSignalBlocker b(*spin_);
        spin_->setValue(v);
    }
    if (slider_) {
        QSignalBlocker b(*slider_);
        slider_->setValue(static_cast<int>(qRound(v * scale_)));
    }

    updating_ = false;
    if (emitChange) emit valueChanged(v);
}

void IntegratedValueController::updateSliderRange_()
{
    if (!slider_) return;
    QSignalBlocker b(*slider_);
    const int imin = static_cast<int>(qFloor(min_ * scale_));
    const int imax = static_cast<int>(qCeil (max_ * scale_));
    slider_->setRange(imin, imax);

    // default page step = 10 units
    if (slider_->pageStep() <= 0)
        slider_->setPageStep(10);
}

inline QByteArray IntegratedValueController::int2Bytes(qint16 v, bool littleEndian)
{
    QByteArray out(2, Qt::Uninitialized);

    const quint16 u = static_cast<quint16>(v); // bit-preserving cast

    if (littleEndian) {
        out[0] = static_cast<char>(u & 0xFF);
        out[1] = static_cast<char>((u >> 8) & 0xFF);
    } else {
        out[0] = static_cast<char>((u >> 8) & 0xFF);
        out[1] = static_cast<char>(u & 0xFF);
    }

    return out;
}
