#pragma once
#include <QObject>
#include <QPointer>
#include <QPushButton>

class QSlider;
class QDoubleSpinBox;

/**
 * @brief Links a QSlider and a QDoubleSpinBox to behave as one unified numeric control.
 *
 * - The controller synchronizes values in both directions.
 * - The slider works on integer scale internally (value * scale_).
 * - The spin box provides precise decimal input.
 * - The class does not own or layout these widgets; it only manages synchronization.
 */
class IntegratedValueController : public QObject
{
    Q_OBJECT
public:
    explicit IntegratedValueController(QObject* parent,
                                       QSlider* slider,
                                       QDoubleSpinBox* spin,
                                       QPushButton* centerButton = nullptr,
                                       double step = 0.5);

    // --- Core API ---
    void   setRange(double min, double max);
    void   setValue(double v);
    void   updateValue(bool isPositive = true);
    void   addValue(double addedValue);
    double value() const;
    QByteArray valueAsBytes();

    void setSingleStep(double step);     // sets spin step & adjusts slider scale
    void setSliderPageStep(double step); // sets page step for slider (PgUp/PgDn)
    void setDecimals(int decimals);      // sets spin box display precision

signals:
    void valueChanged(double value);        // emitted once for each logical change
    void valueEditedByUser(double value);   // emitted on user confirmation (release/editingFinished)

private slots:
    void onSpinChanged(double v);
    void onSpinEditingFinished();
    void onSliderChanged(int iv);
    void onSliderReleased();

private:
    void applyToChildren_(double v, bool emitChange);
    void updateSliderRange_();

    inline QByteArray int2Bytes(qint16 v, bool littleEndian = false);

private:
    QPointer<QSlider>        slider_;
    QPointer<QDoubleSpinBox> spin_;
    QPointer<QPushButton> centerButton_;

    bool   updating_ = false;  // prevents recursive updates
    double min_ = 0.0;
    double max_ = 270.0;
    double step_ = 0.5;        // minimal increment
    int    scale_ = 10;        // internal scale (1 / step)
};
