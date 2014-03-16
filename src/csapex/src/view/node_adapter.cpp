/// HEADER
#include <csapex/view/node_adapter.h>

/// COMPONENT
#include <csapex/model/node.h>
#include <csapex/utility/qt_helper.hpp>
#include <csapex/model/node_worker.h>

/// PROJECT
#include <utils_param/range_parameter.h>
#include <utils_param/interval_parameter.h>
#include <utils_param/value_parameter.h>
#include <utils_param/set_parameter.h>
#include <utils_param/bitset_parameter.h>
#include <utils_param/path_parameter.h>
#include <utils_param/trigger_parameter.h>
#include <utils_param/color_parameter.h>

/// SYSTEM
#include <boost/bind.hpp>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QFileDialog>
#include <QColorDialog>
#include <iomanip>
#include <QGroupBox>
#include <QScrollArea>
#include <QListWidget>

using namespace csapex;

NodeAdapter::NodeAdapter()
    : bridge(this), layout_(NULL), is_gui_setup_(false)
{
    QObject::connect(&bridge, SIGNAL(rebuild()), &bridge, SLOT(rebuildEvent()));
}

NodeAdapter::~NodeAdapter()
{
    clear();
}

void NodeAdapter::clear()
{
    Q_FOREACH(const boost::signals2::connection& c, connections) {
        c.disconnect();
    }
    connections.clear();

    Q_FOREACH(QObject* cb, callbacks) {
        qt_helper::Call* call = dynamic_cast<qt_helper::Call*>(cb);
        if(call) {
            call->disconnect();
        }
        cb->deleteLater();
    }
    callbacks.clear();
}

void NodeAdapter::setNode(Node *node)
{
    node_ = node;

    QObject::connect(node, SIGNAL(modelChanged()), &bridge, SLOT(modelChangedEvent()));
}

Node* NodeAdapter::getNode()
{
    return node_;
}

void NodeAdapter::setupUiAgain()
{
    bridge.triggerRebuild();
}

void NodeAdapter::doSetupUi(QBoxLayout *layout)
{
    layout_ = layout;
    if(!is_gui_setup_) {
        is_gui_setup_ = true;
        setupUi(layout_);

        guiChanged();
    }
}

namespace {
void updateColor(param::ColorParameter *p)
{
    std::vector<int> c = p->value();
    QColor init(c[0], c[1], c[2]);
    QColor color = QColorDialog::getColor(init);
    if (color.isValid()) {
        std::vector<int> v(3);
        v[0] = color.red();
        v[1] = color.green();
        v[2] = color.blue();
        p->set(v);
    }
}

QString toColorSS(const std::vector<int>& v) {
    std::stringstream ss;
    ss << "QPushButton {";
    ss << "background-color: #" << std::hex << std::setfill('0');
    ss << std::setw(2) << v[0];
    ss << std::setw(2) << v[1];
    ss << std::setw(2) << v[2];
    ss << std::dec << ";";
    ss << "}";

    return QString::fromStdString(ss.str());
}
}


void NodeAdapter::setupUi(QBoxLayout * outer_layout)
{
    static std::map<int, boost::function<void(NodeAdapter*, param::Parameter::Ptr)> > mapping_;
    if(mapping_.empty()) {
#define INSTALL(_TYPE_) \
        std::cout << "registering ##_TYPE_ with id " <<  &typeid(_TYPE_) << "(" << typeid(_TYPE_).name() << ")" << std::endl;\
        mapping_[_TYPE_().ID()] = boost::bind(static_cast<void (NodeAdapter::*)( _TYPE_* )> (&NodeAdapter::setupParameter), _1, \
boost::bind(&boost::shared_ptr<_TYPE_>::get, boost::bind(&boost::dynamic_pointer_cast<_TYPE_, param::Parameter>, _2)))

        INSTALL(param::TriggerParameter);
        INSTALL(param::ColorParameter);
        INSTALL(param::PathParameter);
        INSTALL(param::ValueParameter);
        INSTALL(param::RangeParameter);
        INSTALL(param::IntervalParameter);
        INSTALL(param::SetParameter);
        INSTALL(param::BitSetParameter);
#undef INSTALL
    }

    current_layout_ = outer_layout;

    QtHelper::clearLayout(layout_);
    clear();

    std::vector<param::Parameter::Ptr> params = node_->getParameters();

    std::map<std::string, QBoxLayout*> groups;

    Q_FOREACH(param::Parameter::Ptr p, params) {
        param::Parameter* parameter = p.get();

        current_name_= parameter->name();
        current_display_name_ = current_name_;
        std::size_t separator_pos = current_name_.find_first_of('/');
        if(separator_pos != std::string::npos) {
            std::string group = current_name_.substr(0, separator_pos);
            current_display_name_ = current_name_.substr(separator_pos+1);

            if(groups.find(group) != groups.end()) {
                current_layout_ = groups[group];
            } else {
                QGroupBox* gb = new QGroupBox(group.c_str());
                gb->setContentsMargins(0,0,0,0);

                QVBoxLayout* gb_layout = new QVBoxLayout;
                gb->setLayout(gb_layout);
                gb->setCheckable(true);
                gb_layout->setContentsMargins(0,0,0,0);

                current_layout_ = new QVBoxLayout;
                groups.insert(std::make_pair(group, current_layout_));
                current_layout_->setContentsMargins(0,0,0,0);

                QFrame* hider = new QFrame;
                hider->setLayout(current_layout_);
                hider->setContentsMargins(0,0,0,0);
                gb_layout->addWidget(hider);

                outer_layout->addWidget(gb);

                QObject::connect(gb, SIGNAL(toggled(bool)), hider, SLOT(setShown(bool)));
            }
        }

        parameter_enabled(*parameter).disconnect_all_slots();
        parameter_enabled(*parameter).connect(boost::bind(&NodeAdapter::setupUiAgain, this));

        if(!parameter->isEnabled()) {
            continue;
        }

        if(mapping_.find(p->ID()) != mapping_.end()) {
            mapping_[p->ID()](this, p);

        } else {
            current_layout_->addWidget(new QLabel((current_name_ + "'s type is not yet registered (value: " + type2name(p->type()) + ")").c_str()));
        }
    }
}

void NodeAdapter::setupParameter(param::TriggerParameter * trigger_p)
{
    QPushButton* btn = new QPushButton(trigger_p->name().c_str());

    QHBoxLayout* sub = new QHBoxLayout;
    sub->addWidget(btn);
    current_layout_->addLayout(QtHelper::wrap(current_display_name_, sub));

    boost::function<void()> cb = boost::bind(&param::TriggerParameter::trigger, trigger_p);
    qt_helper::Call* call_trigger = new qt_helper::Call(cb);
    callbacks.push_back(call_trigger);

    QObject::connect(btn, SIGNAL(clicked()), call_trigger, SLOT(call()));
}

void NodeAdapter::setupParameter(param::ColorParameter *color_p)
{
    QPushButton* btn = new QPushButton;

    btn->setStyleSheet(toColorSS(color_p->value()));

    QHBoxLayout* sub = new QHBoxLayout;
    sub->addWidget(btn);
    current_layout_->addLayout(QtHelper::wrap(current_display_name_, sub));

    // ui callback
    boost::function<void()> cb = boost::bind(&updateColor, color_p);
    qt_helper::Call* call = new qt_helper::Call(cb);
    callbacks.push_back(call);
    QObject::connect(btn, SIGNAL(clicked()), call, SLOT(call()));

    // model change -> ui
    boost::function<std::vector<int>()> readColor = boost::bind(&param::ColorParameter::value, color_p);
    boost::function<QString()> readSS = boost::bind(&toColorSS, boost::bind(readColor));
    boost::function<void(param::Parameter*)> up = boost::bind(&QPushButton::setStyleSheet, btn, boost::bind(readSS));

    connections.push_back(parameter_changed(*color_p).connect(up));
}

void NodeAdapter::setupParameter(param::PathParameter *path_p)
{
    QLineEdit* path = new QLineEdit;
    QPushButton* open = new QPushButton("open");

    QHBoxLayout* sub = new QHBoxLayout;

    sub->addWidget(path);
    sub->addWidget(open);

    current_layout_->addLayout(QtHelper::wrap(current_display_name_, sub));

    // ui change -> model
    boost::function<std::string(const QString&)> qstring2stdstring = boost::bind(&QString::toStdString, _1);
    boost::function<void()> cb = boost::bind(&NodeAdapter::updateParam<std::string>, this, current_name_, boost::bind(qstring2stdstring, boost::bind(&QLineEdit::text, path)));
    qt_helper::Call* call_set_path = new qt_helper::Call(cb);
    callbacks.push_back(call_set_path);


    boost::function<void()> cb_open = boost::bind(&NodeAdapter::updateParam<std::string>, this, current_name_,
                                                  boost::bind(qstring2stdstring, boost::bind(&QFileDialog::getOpenFileName,
                                                                                             (QWidget*) 0, "Input", "", "All files (*.*)", (QString*) 0, (QFlags<QFileDialog::Option>) 0)));
    qt_helper::Call* call_open = new qt_helper::Call(cb_open);
    callbacks.push_back(call_open);

    // model change -> ui
    boost::function<QString(const std::string&)> stdstring2qstring = boost::bind(&QString::fromStdString, _1);
    boost::function<void(const std::string&)> set = boost::bind(&QLineEdit::setText, path, boost::bind(stdstring2qstring, _1));
    connections.push_back(parameter_changed(*path_p).connect(boost::bind(&NodeAdapter::updateUi<std::string>, this, _1, set)));

    QObject::connect(path, SIGNAL(returnPressed()), call_set_path, SLOT(call()));
    QObject::connect(open, SIGNAL(clicked()), call_open, SLOT(call()));
}

void NodeAdapter::setupParameter(param::ValueParameter *value_p)
{
    if(value_p->is<std::string>()) {
        QLineEdit* txt_ = new QLineEdit;
        txt_->setText(value_p->as<std::string>().c_str());
        QPushButton* send = new QPushButton("set");

        QHBoxLayout* sub = new QHBoxLayout;

        sub->addWidget(txt_);
        sub->addWidget(send);

        current_layout_->addLayout(QtHelper::wrap(current_display_name_, sub));

        // ui change -> model
        boost::function<std::string(const QString&)> qstring2stdstring = boost::bind(&QString::toStdString, _1);
        boost::function<void()> cb = boost::bind(&NodeAdapter::updateParam<std::string>, this, current_name_, boost::bind(qstring2stdstring, boost::bind(&QLineEdit::text, txt_)));
        qt_helper::Call* call = new qt_helper::Call(cb);
        callbacks.push_back(call);

        // model change -> ui
        boost::function<QString(const std::string&)> stdstring2qstring = boost::bind(&QString::fromStdString, _1);
        boost::function<void(const std::string&)> set = boost::bind(&QLineEdit::setText, txt_, boost::bind(stdstring2qstring, _1));
        connections.push_back(parameter_changed(*value_p).connect(boost::bind(&NodeAdapter::updateUi<std::string>, this, _1, set)));

        QObject::connect(txt_, SIGNAL(returnPressed()), call, SLOT(call()));
        QObject::connect(send, SIGNAL(clicked()), call, SLOT(call()));

    } else if(value_p->is<bool>()) {
        QCheckBox* box = new QCheckBox;
        box->setChecked(value_p->as<bool>());

        current_layout_->addLayout(QtHelper::wrap(current_display_name_, box));

        // ui change -> model
        boost::function<void()> cb = boost::bind(&NodeAdapter::updateParam<bool>, this, current_name_, boost::bind(&QCheckBox::isChecked, box));
        qt_helper::Call* call = new qt_helper::Call(cb);
        callbacks.push_back(call);

        // model change -> ui
        boost::function<void(bool)> set = boost::bind(&QCheckBox::setChecked, box, _1);
        connections.push_back(parameter_changed(*value_p).connect(boost::bind(&NodeAdapter::updateUi<bool>, this, _1, set)));

        QObject::connect(box, SIGNAL(toggled(bool)), call, SLOT(call()));

    } else {
        current_layout_->addWidget(new QLabel((current_name_ + "'s type is not yet implemented (value: " + type2name(value_p->type()) + ")").c_str()));
    }
}

void NodeAdapter::setupParameter(param::RangeParameter *range_p)
{
    if(range_p->is<int>()) {
        QIntSlider* slider = QtHelper::makeIntSlider(current_layout_, current_display_name_ , range_p->def<int>(), range_p->min<int>(), range_p->max<int>(), range_p->step<int>(), node_->getCommandDispatcher());
        slider->setIntValue(range_p->as<int>());

        // ui change -> model
        boost::function<void()> cb = boost::bind(&NodeAdapter::updateParam<int>, this, current_name_, boost::bind(&QIntSlider::intValue, slider));
        qt_helper::Call* call = new qt_helper::Call(cb);
        callbacks.push_back(call);

        // model change -> ui
        boost::function<void(int)> set = boost::bind(&QIntSlider::setIntValue, slider, _1);
        connections.push_back(parameter_changed(*range_p).connect(boost::bind(&NodeAdapter::updateUi<int>, this, _1, set)));

        QObject::connect(slider, SIGNAL(intValueChanged(int)), call, SLOT(call()));

    } else if(range_p->is<double>()) {
        QDoubleSlider* slider = QtHelper::makeDoubleSlider(current_layout_, current_display_name_ , range_p->def<double>(), range_p->min<double>(), range_p->max<double>(), range_p->step<double>());
        slider->setDoubleValue(range_p->as<double>());

        // ui change -> model
        boost::function<void()> cb = boost::bind(&NodeAdapter::updateParam<double>, this, current_name_, boost::bind(&QDoubleSlider::doubleValue, slider));
        qt_helper::Call* call = new qt_helper::Call(cb);
        callbacks.push_back(call);

        // model change -> ui
        boost::function<void(double)> set = boost::bind(&QDoubleSlider::setDoubleValue, slider, _1);
        connections.push_back(parameter_changed(*range_p).connect(boost::bind(&NodeAdapter::updateUi<double>, this, _1, set)));

        QObject::connect(slider, SIGNAL(valueChanged(double)), call, SLOT(call()));

    } else {
        current_layout_->addWidget(new QLabel((current_name_ + "'s type is not yet implemented (range: " + type2name(range_p->type()) + ")").c_str()));
    }
}

void NodeAdapter::setupParameter(param::IntervalParameter *interval_p)
{
    if(interval_p->is<std::pair<int, int> >()) {
        const std::pair<int,int>& v = interval_p->as<std::pair<int,int> >();
        QxtSpanSlider* slider = QtHelper::makeSpanSlider(current_layout_, current_display_name_, v.first, v.second, interval_p->min<int>(), interval_p->max<int>());

        // ui change -> model
        boost::function<int()> low = boost::bind(&QxtSpanSlider::lowerValue, slider);
        boost::function<int()> high = boost::bind(&QxtSpanSlider::upperValue, slider);
        boost::function<std::pair<int,int>()> mkpair(boost::bind(&std::make_pair<int,int>, boost::bind(low), boost::bind(high)));
        boost::function<void()> cb = boost::bind(&NodeAdapter::updateParam<std::pair<int,int> >, this, current_name_, boost::bind(mkpair));
        qt_helper::Call* call = new qt_helper::Call(cb);
        callbacks.push_back(call);

        // model change -> ui
        boost::function<void(int)> setLow = boost::bind(&QxtSpanSlider::setLowerValue, slider, _1);
        boost::function<void(int)> setHigh = boost::bind(&QxtSpanSlider::setUpperValue, slider, _1);
        boost::function<void(std::pair<int,int>)> setLowFromPair = boost::bind(setLow, boost::bind(&std::pair<int,int>::first, _1));
        boost::function<void(std::pair<int,int>)> setHighFromPair = boost::bind(setHigh, boost::bind(&std::pair<int,int>::second, _1));

        connections.push_back(parameter_changed(*interval_p).connect(boost::bind(&NodeAdapter::updateUi<std::pair<int,int> >, this, _1, setLowFromPair)));
        connections.push_back(parameter_changed(*interval_p).connect(boost::bind(&NodeAdapter::updateUi<std::pair<int,int> >, this, _1, setHighFromPair)));

        QObject::connect(slider, SIGNAL(lowerValueChanged(int)), call, SLOT(call()));
        QObject::connect(slider, SIGNAL(upperValueChanged(int)), call, SLOT(call()));

    } else if(interval_p->is<std::pair<double, double> >()) {
        const std::pair<double,double>& v = interval_p->as<std::pair<double,double> >();
        QxtDoubleSpanSlider* slider = QtHelper::makeDoubleSpanSlider(current_layout_, current_display_name_, v.first, v.second, interval_p->min<double>(), interval_p->max<double>(), interval_p->step<double>());

        // ui change -> model
        boost::function<double()> low = boost::bind(&QxtDoubleSpanSlider::lowerDoubleValue, slider);
        boost::function<double()> high = boost::bind(&QxtDoubleSpanSlider::upperDoubleValue, slider);
        boost::function<std::pair<double,double>()> mkpair(boost::bind(&std::make_pair<double,double>, boost::bind(low), boost::bind(high)));
        boost::function<void()> cb = boost::bind(&NodeAdapter::updateParam<std::pair<double,double> >, this, current_name_, boost::bind(mkpair));
        qt_helper::Call* call = new qt_helper::Call(cb);
        callbacks.push_back(call);

        // model change -> ui
        boost::function<void(double)> setLow = boost::bind(&QxtDoubleSpanSlider::setLowerDoubleValue, slider, _1);
        boost::function<void(double)> setHigh = boost::bind(&QxtDoubleSpanSlider::setUpperDoubleValue, slider, _1);
        boost::function<void(std::pair<double,double>)> setLowFromPair = boost::bind(setLow, boost::bind(&std::pair<double,double>::first, _1));
        boost::function<void(std::pair<double,double>)> setHighFromPair = boost::bind(setHigh, boost::bind(&std::pair<double,double>::second, _1));

        connections.push_back(parameter_changed(*interval_p).connect(boost::bind(&NodeAdapter::updateUi<std::pair<double,double> >, this, _1, setLowFromPair)));
        connections.push_back(parameter_changed(*interval_p).connect(boost::bind(&NodeAdapter::updateUi<std::pair<double,double> >, this, _1, setHighFromPair)));

        QObject::connect(slider, SIGNAL(lowerValueChanged(int)), call, SLOT(call()));
        QObject::connect(slider, SIGNAL(upperValueChanged(int)), call, SLOT(call()));


    } else {
        current_layout_->addWidget(new QLabel((current_name_ + "'s type is not yet implemented (inverval: " + type2name(interval_p->type()) + ")").c_str()));
    }
}

void NodeAdapter::setupParameter(param::SetParameter *set_p)
{
    QComboBox* combo = new QComboBox;

    updateUiSetScope(set_p, combo);
    current_layout_->addLayout(QtHelper::wrap(current_display_name_, combo));

    // ui change -> model
    boost::function<std::string(const QString&)> qstring2stdstring = boost::bind(&QString::toStdString, _1);
    boost::function<void()> cb = boost::bind(&NodeAdapter::updateParamSet, this, current_name_, boost::bind(qstring2stdstring, boost::bind(&QComboBox::currentText, combo)));
    qt_helper::Call* call = new qt_helper::Call(cb);
    callbacks.push_back(call);

    // model change -> ui
    boost::function<QString(const std::string&)> stdstring2qstring = boost::bind(&QString::fromStdString, _1);
    boost::function<int(const QString&)> txt2idx = boost::bind(&QComboBox::findData, combo, _1, Qt::DisplayRole, static_cast<Qt::MatchFlags>(Qt::MatchExactly|Qt::MatchCaseSensitive));
    boost::function<void(const QString&)> select = boost::bind(&QComboBox::setCurrentIndex, combo, boost::bind(txt2idx, _1));
    boost::function<void(const std::string&)> set = boost::bind(select, boost::bind(stdstring2qstring, _1));
    connections.push_back(parameter_changed(*set_p).connect(boost::bind(&NodeAdapter::updateUiSet, this, _1, set)));

    connections.push_back(scope_changed(*set_p).connect(boost::bind(&NodeAdapter::updateUiSetScope, this, set_p, combo)));

    QObject::connect(combo, SIGNAL(currentIndexChanged(QString)), call, SLOT(call()));
}

void NodeAdapter::setupParameter(param::BitSetParameter *bitset_p)
{
    QGroupBox* group = new QGroupBox(current_name_.c_str());
    QVBoxLayout* l = new QVBoxLayout;
    group->setLayout(l);
    for(int i = 0; i < bitset_p->noParameters(); ++i) {
        std::string str = bitset_p->getName(i);
        QCheckBox* item = new QCheckBox(QString::fromStdString(str));
        l->addWidget(item);
        if(bitset_p->isSet(str)) {
            item->setChecked(true);
        }

        // ui change -> model
        boost::function<bool()> isChecked = boost::bind(&QCheckBox::isChecked, item);
        boost::function<void()> cb = boost::bind(&param::BitSetParameter::setBitTo, bitset_p, str, boost::bind(isChecked), false);
        qt_helper::Call* call = new qt_helper::Call(cb);
        callbacks.push_back(call);
        QObject::connect(item, SIGNAL(toggled(bool)), call, SLOT(call()));

        // model change -> ui
        boost::function<bool()> isSet = boost::bind(&param::BitSetParameter::isSet, bitset_p, str);
        connections.push_back(parameter_changed(*bitset_p).connect(boost::bind(&QCheckBox::setChecked, item, boost::bind(isSet))));
    }
    current_layout_->addWidget(group);
}

template <typename T>
void NodeAdapter::updateParam(const std::string& name, T value)
{
    param::Parameter* p = node_->getParameter(name).get();
    p->set<T>(value);
    guiChanged();
}

void NodeAdapter::updateParamSet(const std::string& name, const std::string& value)
{
    param::SetParameter::Ptr set_p = node_->getParameter<param::SetParameter>(name);
    if(set_p) {
        set_p->setByName(value);
        guiChanged();
    }
}

void NodeAdapter::updateParamBitSet(const std::string& name, const QListView* list)
{
    param::BitSetParameter::Ptr bitset_p = node_->getParameter<param::BitSetParameter>(name);
    if(bitset_p) {
        std::vector<std::string> selected;
        Q_FOREACH(const QModelIndex& idx, list->selectionModel()->selectedIndexes()) {
            selected.push_back(idx.data().toString().toStdString());
        }
        bitset_p->setBits(selected, true);
        guiChanged();
    }
}

template <typename T>
void NodeAdapter::updateUi(const param::Parameter* p, boost::function<void(T)> setter)
{
    setter(node_->param<T>(p->name()));
}


void NodeAdapter::updateUiSet(const param::Parameter *p, boost::function<void (const std::string &)> setter)
{
    const param::SetParameter* set_p = dynamic_cast<const param::SetParameter*> (p);
    if(set_p) {
        setter(set_p->getName());
    }
}

void NodeAdapter::updateUiSetScope(const param::SetParameter *set_p, QComboBox *combo)
{
    int current = 0;
    combo->clear();
    std::string selected;
    try {
        selected = set_p->getName();
    } catch(const std::exception& e) {
        selected = "";
    }

    for(int i = 0; i < set_p->noParameters(); ++i) {
        std::string str = set_p->getName(i);
        combo->addItem(QString::fromStdString(str));

        if(str == selected) {
            current = i;
        }
    }
    combo->setCurrentIndex(current);
}

void NodeAdapter::updateUiBitSet(const param::Parameter *p, const QListView *list)
{
    const param::BitSetParameter* bitset_p = dynamic_cast<const param::BitSetParameter*> (p);
    if(bitset_p) {
        for(int i = 0; i < bitset_p->noParameters(); ++i) {
            QModelIndex idx = list->model()->index(i,0);
            std::string str = bitset_p->getName(i);
            list->selectionModel()->select(idx, bitset_p->isSet(str) ? QItemSelectionModel::Select : QItemSelectionModel::Deselect);
        }
    }
}
void NodeAdapter::updateDynamicGui(QBoxLayout *)
{

}

void NodeAdapter::modelChangedEvent()
{

}

void NodeAdapter::guiChanged()
{
    bridge.triggerGuiChanged();
}

NodeAdapterBridge::NodeAdapterBridge(NodeAdapter *parent)
    : parent_(parent)
{

}

void NodeAdapterBridge::modelChangedEvent()
{
    parent_->modelChangedEvent();
}

void NodeAdapterBridge::rebuildEvent()
{
    parent_->setupUi(parent_->layout_);
}

void NodeAdapterBridge::triggerGuiChanged()
{
    Q_EMIT guiChanged();
}

void NodeAdapterBridge::triggerRebuild()
{
    Q_EMIT rebuild();
}
