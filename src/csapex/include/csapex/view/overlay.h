#ifndef OVERLAY_H
#define OVERLAY_H

/// COMPONENT
#include <csapex/csapex_fwd.h>

/// SYSTEM
#include <map>
#include <QWidget>
#include <QPainter>

namespace csapex
{

class Overlay : public QWidget
{
    Q_OBJECT

public:
    Overlay(QWidget* parent = 0);

public Q_SLOTS:
    void addTemporaryConnection(Connector* from, Connector* to);
    void addTemporaryConnection(Connector *from, const QPoint& end);
    void deleteTemporaryConnections();
    void deleteTemporaryConnectionsAndRepaint();

    void connectionAdded(Connection*);
    void connectionDeleted(Connection*);

    void fulcrumAdded(Connection*);
    void fulcrumMoved(Connection*);
    void fulcrumDeleted(Connection*);

    void tick();

    void invalidateSchema();
    void refresh();

public:
    bool mouseMoveEventHandler(bool drag, QMouseEvent * e);
    bool mousePressEventHandler(QMouseEvent * e);
    bool mouseReleaseEventHandler(QMouseEvent * e);

    bool keyPressEventHandler(QKeyEvent* e);
    bool keyReleaseEventHandler(QKeyEvent* e);

    void setSelectionRectangle(const QPoint& a, const QPoint& b);

    bool hasTempConnectionHandle() const;
    bool showContextMenu(const QPoint& pos);

protected:
    bool showConnectionContextMenu(const QPoint& pos);
    bool showFulcrumContextMenu(const QPoint& pos);

    void drawActivity(int life, Connector* c);
    void drawConnector(Connector* c);
    void drawConnection(Connection& connection);

    void drawConnection(const QPoint& from, const QPoint& to, int id);

    void paintEvent(QPaintEvent* event);
    void resizeEvent(QResizeEvent * event);

    QPen makeLinePen(const QPoint &from, const QPoint &to);
    QPen makeSelectedLinePen(const QPoint &from, const QPoint &to);

protected:
    struct TempConnection {
        Connector* from;
        QPoint to;
    };

    struct CurrentConnectionState {
        bool selected;
        bool highlighted;
        bool error;
        bool disabled;
        bool minimized_from;
        bool minimized_to;
        bool minimized;

        double r;
    };

    CurrentConnectionState ccs;

    std::vector<TempConnection> temp_;

    QPainter* painter;
    QPainter* schematics_painter;
    QImage schematics;

    QTimer* repainter;

    int activity_marker_min_width_;
    int activity_marker_max_width_;
    int activity_marker_min_opacity_;
    int activity_marker_max_opacity_;

    QColor color_connection;

    QColor color_connected;
    QColor color_disconnected;

    QColor color_in_connected;
    QColor color_in_disconnected;

    QColor color_out_connected;
    QColor color_out_disconnected;


    int highlight_connection_id_;
    bool schema_dirty_;

    int connector_radius_;

    QPoint selection_a;
    QPoint selection_b;

    QPoint drag_connection_handle_;
    QPoint current_splicing_handle_;
    int drag_sub_section_;
    int drag_connection_;
    bool fulcrum_is_hovered_;

    bool splicing_requested;
    bool splicing;
};

}
#endif // OVERLAY_H