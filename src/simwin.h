#ifndef SIMWIN_H
#define SIMWIN_H

#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QMessageBox>
#include <QFileDialog>
#include <QSocketNotifier>
#include <QModelIndexList>
#include <QStandardItemModel>
#include <QMouseEvent>
#include "bmp_tlacitka.h"
#include "modules/module.h"
#include <QDebug>

QT_BEGIN_NAMESPACE
namespace Ui { class Tsimwin; }
QT_END_NAMESPACE

extern std::array<std::unique_ptr<MtbModule>, Mtb::_MAX_MODULES> modules;

struct mtbConnection {
    int addr;
    int pin;
};

class TprusvAZD : public QLabel
{
    Q_OBJECT
public:
    TprusvAZD(int _X, int _Y, QWidget *parent = nullptr);
    enum prusvState { psZakladni, psBila, psCervena };

    struct mtbConnection mtbBila;
    struct mtbConnection mtbCervena;

private:
    int X,Y;
    enum prusvState state;

public slots:
    void lampChanged(enum prusvState  _state);
};

class TtlacAZD : public QPushButton
{
    Q_OBJECT
public:
    enum tlacColor { none, zelena, bila };
    enum tlacState { zakladni, macknute, vytazene };

//    TtlacAZD();

    struct mtbConnection mtbPushIn;
    struct mtbConnection mtbPullIn;
    struct mtbConnection mtbLampOut;

    TtlacAZD(int _X, int _Y, bool _canPush = true, bool _canPull = false, bool _aretacePush = false, bool _aretacePull = false , enum tlacColor _color = none, QWidget *parent = nullptr);
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void setIconState();

    enum tlacState stateM;
    bool stateLit;

private:
    bool canPush;
    bool canPull;
    bool aretacePush;
    bool aretacePull;
    enum tlacColor color;
    int X,Y;
    QIcon *ico;

    void setState(enum tlacState state);

signals:
    void contactChanged(int mtbaddr, int mtbpin, bool state);

public slots:
    void lampChanged(bool state);


};


class Tsimwin : public QMainWindow
{
    Q_OBJECT
public:
    explicit Tsimwin(QWidget *parent = nullptr);
    // seznamy prvků na pultu
    QList<TtlacAZD*> prvTlac; // tlačítka
    QList<TprusvAZD*> prvPrusv; // průsvitky
    QList<QLabel*> prvLabel; // cedulky

private:
    QMainWindow *ui;

signals:
};

#endif // SIMWIN_H
