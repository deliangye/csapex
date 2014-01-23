#ifndef COLOR_PARAMETER_H
#define COLOR_PARAMETER_H

/// COMPONENT
#include <utils_param/value_parameter.h>

namespace param {


class ColorParameter : public Parameter
{
    friend class boost::serialization::access;
    friend class ParameterFactory;

public:
    typedef boost::any variant;

public:
    typedef boost::shared_ptr<ColorParameter> Ptr;

public:
    friend YAML::Emitter& operator << (YAML::Emitter& e, const ColorParameter& p) {
        p.write(e);
        return e;
    }
    friend YAML::Emitter& operator << (YAML::Emitter& e, const ColorParameter::Ptr& p) {
        p->write(e);
        return e;
    }

    friend void operator >> (const YAML::Node& node, param::ColorParameter& value) {
        value.read(node);
    }

    friend void operator >> (const YAML::Node& node, param::ColorParameter::Ptr& value) {
        if(!value) {
            value.reset(new ColorParameter);
        }
        value->read(node);
    }

public:
    ColorParameter();
    explicit ColorParameter(const std::string& name, int r, int g, int b); virtual const std::type_info &type() const;

    virtual std::string toStringImpl() const;

    void setFrom(const Parameter& other);
    void set(const std::vector<int> &v);

    void write(YAML::Emitter& e) const;
    void read(const YAML::Node& n);

    std::vector<int> def() const;
    std::vector<int> value() const;

    template<class Archive>
    void serialize(Archive& ar, const unsigned int /*file_version*/) {
        ar & colors_;
    }

protected:
    virtual boost::any get_unsafe() const;
    virtual void set_unsafe(const boost::any& v);

private:
    std::vector<int> colors_;
    std::vector<int> def_;
};

}

#endif // COLOR_PARAMETER_H
