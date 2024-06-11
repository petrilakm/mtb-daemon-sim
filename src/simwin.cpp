#include "simwin.h"
#include "qregularexpression.h"

TtlacAZD::TtlacAZD(int _X, int _Y, bool _canPush, bool _canPull, bool _aretacePush, bool _aretacePull, enum tlacColor _color, QWidget *parent) : QPushButton(parent)
{
    //setParent(parent);
    setText("");
    X = _X;
    Y = _Y;
    canPush = _canPush;
    canPull = _canPull;
    aretacePush = _aretacePush;
    aretacePull = _aretacePull;
    stateM = zakladni;
    stateLit = 0;
    color = _color;
    setGeometry(X, Y, 15,15);
    setStyleSheet("background-color: red;");
    //setIcon(*itb0z);
    setFlat(true);
    setIconState();
    QString toolTip;
    if (canPush) {
        toolTip += "mačkací";
        if (aretacePush) toolTip += " s aretací";
        toolTip += "\n";
    }
    if (canPull) {
        toolTip += "tahací";
        if (aretacePull) toolTip += " s aretací";
        toolTip += "\n";
    }
    setToolTip(toolTip);
    show();
}

void TtlacAZD::setState(enum tlacState state)
{
    if (stateM != state) {
        if (state == zakladni) {
            if (mtbPushIn.addr >= 0) emit contactChanged(mtbPushIn.addr, mtbPushIn.pin, 0);
            if (mtbPullIn.addr >= 0) emit contactChanged(mtbPullIn.addr, mtbPullIn.pin, 0);
        }
        if (state == macknute) {
            if (mtbPushIn.addr >= 0) emit contactChanged(mtbPushIn.addr, mtbPushIn.pin, 1);
            if (mtbPullIn.addr >= 0) emit contactChanged(mtbPullIn.addr, mtbPullIn.pin, 0);
        }
        if (state == vytazene) {
            if (mtbPushIn.addr >= 0) emit contactChanged(mtbPushIn.addr, mtbPushIn.pin, 0);
            if (mtbPullIn.addr >= 0) emit contactChanged(mtbPullIn.addr, mtbPullIn.pin, 1);
        }

        stateM = state;
        setIconState();
    }
}

void TtlacAZD::mousePressEvent(QMouseEvent *event)
{
    //QPoint lastPoint = event->position().toPoint();
    if ((event->button() == Qt::LeftButton) && canPush) {
        if (!aretacePush) {
            setState(macknute);
        } else {
            if (stateM == macknute) {
                setState(zakladni);
            } else {
                setState(macknute);
            }
        }
    }
    if ((event->button() == Qt::RightButton) && canPull) {
        if (!aretacePush) {
            setState(vytazene);
        } else {
            if (stateM == vytazene) {
                setState(zakladni);
            } else {
                setState(vytazene);
            }
        }
    }
}

void TtlacAZD::mouseReleaseEvent(QMouseEvent *event)
{
    if (stateM == macknute && !(aretacePush)) setState(zakladni);
    if (stateM == vytazene && !(aretacePull)) setState(zakladni);
}

void TtlacAZD::lampChanged(bool state)
{
    stateLit = state;
    setIconState();
}

void TtlacAZD::setIconState()
{
    QIcon *IconNormal = nullptr;
    QIcon *IconPushed = nullptr;
    QIcon *IconPulled = nullptr;

    // určí obrázky podle typu průsvitky
    switch (color) {
    case bila:
        if (stateLit) {
            IconNormal = itb1z;
            IconPushed = itb1m;
            IconPulled = itb1v;
        } else {
            IconNormal = itb0z;
            IconPushed = itb0m;
            IconPulled = itb0v;
        }
        break;
    case zelena:
        if (stateLit) {
            IconNormal = itz1z;
            IconPushed = itz1m;
            IconPulled = itz1v;
        } else {
            IconNormal = itz0z;
            IconPushed = itz0m;
            IconPulled = itz0v;
        }
        break;
    default:
        IconNormal = itn0z;
        IconPushed = itn0m;
        IconPulled = itn0v;
    }

    // nastavi podle zmačknutí/vytažení obrázek
    switch (stateM) {
    case macknute:
        setIcon(*IconPushed);
        break;
    case vytazene:
        setIcon(*IconPulled);
        break;
    default:
        setIcon(*IconNormal);
    }
}

TprusvAZD::TprusvAZD(int _X, int _Y, QWidget *parent)
{
    X = _X;
    Y = _Y;
    setGeometry(X, Y, 32, 12);
    setParent(parent);
    setText("");
    setPixmap(*iu0);
}

void TprusvAZD::lampChanged(enum prusvState _state)
{
    state = _state;

    switch (state) {
    case psBila:
        setPixmap(*iu0);
        break;
    case psCervena:
        setPixmap(*iu0);
        break;
    default:
        setPixmap(*iu0);
    }
}

Tsimwin::Tsimwin(QWidget *parent)
    : QMainWindow{parent}
{
    QLabel *plabel;
    TtlacAZD *pt;
    TprusvAZD *pprusv;

    // create window
    ui = new QMainWindow(nullptr);
    ui->setGeometry(100,200,600,400);

    // load buttons images
    itb0z = new QIcon(":/bmp/tb0z");
    itb0m = new QIcon(":/bmp/tb0m");
    itb0v = new QIcon(":/bmp/tb0v");
    itb1z = new QIcon(":/bmp/tb1z");
    itb1m = new QIcon(":/bmp/tb1m");
    itb1v = new QIcon(":/bmp/tb1v");
    itz0z = new QIcon(":/bmp/tz0z");
    itz0m = new QIcon(":/bmp/tz0m");
    itz0v = new QIcon(":/bmp/tz0v");
    itz1z = new QIcon(":/bmp/tz1z");
    itz1m = new QIcon(":/bmp/tz1m");
    itz1v = new QIcon(":/bmp/tz1v");
    itn0z = new QIcon(":/bmp/tnz");
    itn0m = new QIcon(":/bmp/tnm");
    itn0v = new QIcon(":/bmp/tnv");

    iu0 = new QPixmap(":/bmp/u0");
    iu1 = new QPixmap(":/bmp/u1");
    iu2 = new QPixmap(":/bmp/u2");

    // load definition file
    QStringList linelist;
    QStringList coordlist;
    QString coord;
    int cx, cy;
    QString type;
    QString param;
    bool tlacCanPush;
    bool tlacCanPull;
    bool tlacAretacePush;
    bool tlacAretacePull;
    TtlacAZD::tlacColor tlacColor;
    QStringList paramlist;
    QStringList mtbdesig;
    struct mtbConnection mtbArray[3];
    QFile inputFile("gui.csv");
    if (inputFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&inputFile);
        while (!in.atEnd())
        {
            QString line = in.readLine();
            if (line == "##") {

            }
            linelist = line.split(";");
            if (linelist.count() > 2) {
                type = linelist.at(0);
                coord =linelist.at(1);
                param =linelist.at(2);
                coordlist = coord.split(',');
                if (coordlist.count() == 2) {
                    cx = coordlist.at(0).toInt();
                    cy = coordlist.at(1).toInt();
                } else {
                    cx = 0; cy = 0;
                }


                if (type == "l") {
                    // static label
                    plabel = new QLabel(ui);
                    plabel->setGeometry(cx,cy,20,12);
                    plabel->setText(param);
                    plabel->adjustSize();
                    prvLabel.append(plabel);
                }
                if (type[0] == 't' && type.length() == 4) {
                    // tlacitko
                    // determine type
                    tlacCanPush = ((type[1] == 'm') || (type[1] == 'M'));
                    tlacCanPull = ((type[2] == 't') || (type[2] == 'T'));
                    tlacAretacePush = (type[1] == 'M');
                    tlacAretacePull = (type[2] == 'T');
                    switch (type[3].toLatin1()) {
                        case 'b': tlacColor = TtlacAZD::bila; break;
                        case 'z': tlacColor = TtlacAZD::zelena; break;
                        default:  tlacColor = TtlacAZD::none; break;
                    }
                    //  read mtb list
                    paramlist = param.split(',');
                    for(int i = 0; i < 3; i++) {
                        mtbArray[i].addr = -1;
                        mtbArray[i].pin = -1;
                        if (i < paramlist.count()) {
                            mtbdesig = paramlist.at(i).split('/');
                            if (mtbdesig.count() == 2) {
                                mtbArray[i].addr = mtbdesig.at(0).toInt();
                                mtbArray[i].pin  = mtbdesig.at(1).toInt();
                            }
                        }
                    }
                    // create new button
                    pt = new TtlacAZD(cx,cy,tlacCanPush, tlacCanPull, tlacAretacePush, tlacAretacePull, tlacColor, ui);
                    pt->mtbPushIn = mtbArray[0];
                    pt->mtbPullIn = mtbArray[1];
                    pt->mtbLampOut = mtbArray[2];
                    prvTlac.append(pt);
                }
                if (type == "p") {
                    // průsvitka
                    pprusv = new TprusvAZD(cx, cy, ui);
                    //  read mtb list
                    paramlist = param.split(',');
                    for(int i = 0; i < 2; i++) {
                        mtbArray[i].addr = -1;
                        mtbArray[i].pin = -1;
                        if (i < paramlist.count()) {
                            mtbdesig = paramlist.at(i).split('/');
                            if (mtbdesig.count() == 2) {
                                mtbArray[i].addr = mtbdesig.at(0).toInt();
                                mtbArray[i].pin  = mtbdesig.at(1).toInt();
                            }
                        }
                    }
                    pprusv->mtbBila = mtbArray[0];
                    pprusv->mtbCervena = mtbArray[1];
                    prvPrusv.append(pprusv);
                }


            }
        }
        inputFile.close();
    }


    //if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    //    throw ConfigNotFound(QString("GUI configuration file not found!"));




    ui->show();
    //
}
/*
void Tsimwin::doConnect()
{
    TtlacAZD *pT;
    for(int i = 0; i < prvTlac.count(); i++) {
        pT = prvTlac.at(i);
        if (pT->mtbPushIn.addr >= 0) {
            connect(pT, SIGNAL(contactChanged(int,int,bool)), )
        }
    }
}
*/
