#ifndef FILTER_HISTOGRAM_H
#define FILTER_HISTOGRAM_H

class QDoubleSlider;
class QCheckBox;
class QComboBox;
class QSlider;

/// COMPONENT
#include <vision_evaluator/filter.h>

namespace vision_evaluator {
class Histogram : public vision_evaluator::BoxedObject
{
    Q_OBJECT

public:
    Histogram();

    void         setState(Memento::Ptr memento);
    Memento::Ptr getState();

    virtual void fill(QBoxLayout* layout);

private Q_SLOTS:
    void messageArrived(ConnectorIn* source);
    void selectedPreset(QString text);
    void enableNorm(bool value);
    void enableScale(bool value);

private:
    ConnectorIn   *input_;
    ConnectorOut  *output_;
    ConnectorOut  *output_histogram_;
    QBoxLayout    *layout_;
    QSlider       *slide_lightness_;
    QDoubleSlider *slide_ch_zoom_;
    QWidget       *zoom_container_;
    QCheckBox     *check_equal_;

    std::vector<cv::Scalar> colors_;


    class State : public Memento {
    public:
        void readYaml(const YAML::Node &node)
        {
            node["ch1_min"] >> ch1_min;
            node["ch1_max"] >> ch1_max;
            node["ch2_min"] >> ch2_min;
            node["ch2_max"] >> ch2_max;
            node["ch3_min"] >> ch3_min;
            node["ch3_max"] >> ch3_max;
        }

        void writeYaml(YAML::Emitter &out) const
        {
            out << YAML::Key << "ch1_min" << YAML::Value << ch1_min;
            out << YAML::Key << "ch1_max" << YAML::Value << ch1_max;
            out << YAML::Key << "ch2_min" << YAML::Value << ch2_min;
            out << YAML::Key << "ch2_max" << YAML::Value << ch2_max;
            out << YAML::Key << "ch3_min" << YAML::Value << ch3_min;
            out << YAML::Key << "ch3_max" << YAML::Value << ch3_max;
        }

    public:
        double ch1_min;
        double ch1_max;
        double ch2_min;
        double ch2_max;
        double ch3_min;
        double ch3_max;


    };

    State state_;

};
}
#endif // FILTER_HISTOGRAM_H