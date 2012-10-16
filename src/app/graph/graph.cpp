/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Build Suite.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#include <app/shared/commandlineparser.h>
#include <buildgraph/artifact.h>
#include <buildgraph/buildgraph.h>
#include <language/qbsengine.h>
#include <language/sourceproject.h>
#include <logging/consolelogger.h>
#include <logging/logger.h>
#include <tools/fileinfo.h>

#include <cassert>

#include <QCoreApplication>
#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <cmath>

#include <QGraphicsView>
#include <QWheelEvent>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QFont>
#include <QStyle>
#include <QLineEdit>
#include <QGridLayout>
#include <QAbstractListModel>
#include <QTimer>

class QGV : public QGraphicsView
{
public:
    QGV(QGraphicsScene *scene) : QGraphicsView(scene)
    {
        setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        setDragMode(ScrollHandDrag);
        scaleView(0.5);
    }

protected:
    void wheelEvent ( QWheelEvent * event )
    {
        scaleView(pow((double)2, event->delta() / 240.0));
    }

    void scaleView(qreal scaleFactor)
    {
        qreal factor = transform().scale(scaleFactor, scaleFactor).mapRect(QRectF(0, 0, 1, 1)).width();
        if (factor < 0.07 || factor > 100)
            return;

        scale(scaleFactor, scaleFactor);
    }
};

struct ArtifactC
{
    qbs::Artifact *artifact;
    QString text;
    QString label;
    int grade;
    QRectF rectangle;
    int row;
    int col;
    QGraphicsRectItem *graphicsRectItem;
};

QHash<qbs::Artifact *, ArtifactC *> cartifacts;
const qreal SCALE = 100.0f;
const qreal VSPACE = 200.0f;

class MainWaffl : public QWidget
{
Q_OBJECT
public:
    MainWaffl(QGraphicsScene *scene)
        : grid(this)
        , qgv(scene)
    {
        grid.addWidget(&qgv, 0,0,1,1);
        grid.addWidget(&searchBox, 1,0,1,1);

        connect(&searchBox, SIGNAL(textChanged ( const QString &)),
                this, SLOT(startSearchTimer()));
        connect(&searchTimer, SIGNAL(timeout()),
                this, SLOT(search()));
        l =  cartifacts.values();
        qgv.centerOn(0,0);
    }
private slots:
    void startSearchTimer()
    {
        searchTimer.start(100);
    }
    void search()
    {
        searchTimer.stop();
        foreach (ArtifactC *nc, currentHis) {
            nc->graphicsRectItem->setBrush(QColor(Qt::white));
        }
        currentHis.clear();

        QString t = searchBox.text();
        if (t.isEmpty())
            return;
        foreach (ArtifactC *nc, l) {
            if (nc->label.contains(t)) {
                currentHis.append(nc);
                nc->graphicsRectItem->setBrush(QColor(Qt::red));
                qgv.ensureVisible(nc->graphicsRectItem);
            }
        }
    }
private:
    QList<ArtifactC *> currentHis;
    QTimer searchTimer;
    QGridLayout grid;
    QLineEdit searchBox;
    QGV qgv;
    QList<ArtifactC *> l;
};


void targetToScene(QGraphicsScene *scene, qbs::BuildProduct *t);

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("qbs");
    app.setOrganizationName("Nokia");
    app.setOrganizationDomain("qt.nokia.com");

    // read commandline
    QStringList arguments = app.arguments();
    arguments.removeFirst();

    qbs::ConsoleLogger cl;
    qbs::CommandLineParser parser;
    if (!parser.parseCommandLine(arguments)) {
        parser.printHelp();
        return 1;
    }
    if (parser.isHelpSet()) {
        parser.printHelp();
        return 0;
    }

    qbs::QbsEngine engine;
    qbs::SourceProject sourceProject(&engine);
    try {
        sourceProject.setSettings(parser.settings());
        sourceProject.loadPlugins();
        sourceProject.loadProject(parser.projectFileName(), parser.buildConfigurations());
    } catch (const qbs::Error &error) {
        qbs::qbsError() << error.toString();
        return 4;
    }

    QGraphicsScene scene;
    foreach (const qbs::BuildProject::Ptr &buildProject, sourceProject.buildProjects()) {
        foreach (const qbs::BuildProduct::Ptr &buildProduct, buildProject->buildProducts()) {
            targetToScene(&scene, buildProduct.data());
        }
    }
    QRectF sr = scene.sceneRect();
    sr.adjust(-50, -50, 50, 50);
    scene.setSceneRect(sr);
    MainWaffl waffl(&scene);
    waffl.showMaximized();

    return  app.exec();
}


QMap<int, QMap<int, ArtifactC *> > table;

int startcol = 0;
int endcol = 0;
void step1(qbs::Artifact *artifact, int l)
{
    if (!cartifacts.contains(artifact)) {
        ArtifactC *nc =  new ArtifactC;
        cartifacts.insert(artifact, nc);
        nc->text = artifact->filePath();
        nc->label = artifact->fileName();
        nc->artifact = artifact;
        QMap<int, ArtifactC *> & line = table[l];
        int col = startcol;
        while (line.contains(col)) {
            col++;
        }
        table[l][col] = nc;
        endcol = qMax(endcol, col);
        foreach (qbs::Artifact *n2, artifact->children) {
            step1(n2, l + 1);
        }
        startcol = endcol + 1;
    }
}


QList<ArtifactC *> ncl;
QMap<int, int> layerx;
QMap<int, int> layerC;
QMultiMap<ArtifactC *, ArtifactC *> edges;

void targetToScene(QGraphicsScene *scene, qbs::BuildProduct *t)
{
    QFont sceneFont = scene->font();
    sceneFont.setPointSize(14);
    scene->setFont(sceneFont);

    QPen pen = QColor(Qt::black);
    QBrush brush = QColor(Qt::white);

    foreach (qbs::Artifact *targetArtifact, t->targetArtifacts)
        step1(targetArtifact, 1);
    startcol = endcol + 1;
    QFontMetricsF metrics(sceneFont);

    // draw artifacts
    pen.setWidth(1);
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::MiterJoin);
    for (QMap<int, QMap<int, ArtifactC *> >::const_iterator i = table.constBegin(); i != table.constEnd(); i++) {
        int row = i.key();
        QMap<int, ArtifactC *> line = i.value();
        for (QMap<int, ArtifactC *>::const_iterator j = line.constBegin(); j != line.constEnd(); j++) {
            int col = j.key();
            ArtifactC *nc = j.value();
            QRectF textRect = metrics.boundingRect(nc->label);
            nc->row = row;
            nc->col = col;
            int x = row * (400 + VSPACE);
            int y = col * 200;
            nc->rectangle = textRect;
            nc->rectangle.setWidth(400);
            nc->rectangle.moveCenter(QPointF(x , y));

            QGraphicsRectItem *item =  scene->addRect(nc->rectangle, pen, brush);
            item->setToolTip(nc->text);
            nc->graphicsRectItem = item;

            QGraphicsSimpleTextItem *slabel = scene->addSimpleText(nc->label);
            slabel->setBrush(Qt::black);
            slabel->setFont(sceneFont);

            QRect textrect = QStyle::alignedRect (Qt::LeftToRight,
                    Qt::AlignCenter,
                    textRect.size().toSize(),
                    nc->rectangle.toRect());
            slabel->setPos(textrect.topLeft());

            // insert edges
            foreach (qbs::Artifact *c, nc->artifact->children) {
                edges.insert(nc, cartifacts[c]);
            }
        }
    }

    // draw edges
    pen.setWidth(3);

    for (QMap<ArtifactC *, ArtifactC *>::const_iterator i = edges.constBegin();
            i != edges.constEnd();
            i++) {
        ArtifactC * k = i.key();
        ArtifactC * v = i.value();
        assert(v);
        QPainterPath path;

        QPointF fromP;
        QPointF toP;


        if (k->row < v->row) {
            fromP = QPointF(k->rectangle.x() + k->rectangle.width(), k->rectangle.y() + k->rectangle.height()/2);
            toP =   QPointF(v->rectangle.x(), v->rectangle.y() + v->rectangle.height()/2);
        } else {
            fromP = QPointF(k->rectangle.x() + k->rectangle.width(), k->rectangle.y() + k->rectangle.height()/2);
            toP =   QPointF(v->rectangle.x(), v->rectangle.y() + v->rectangle.height()/2);
        }
            /*
        } else if (k->row > v->row) {
            fromP = QPointF(k->rc.x() + k->rc.width()/2, k->rc.y());
            toP =   QPointF(v->rc.x() + v->rc.width()/2, v->rc.y() + v->rc.height());
        } else if (k->row < v->row) {
            fromP = QPointF(k->rc.x() + k->rc.width()/2, k->rc.y() + v->rc.height());
            toP =   QPointF(v->rc.x() + v->rc.width()/2, v->rc.y() );
        }
        */

        path.moveTo(fromP);

        QPointF c1 = fromP;
        c1.setX(c1.x() + VSPACE/2);
        QPointF c2 = toP;
        c2.setX(c2.x() - VSPACE/2);

        path.cubicTo(c1, c2, toP);

        scene->addPath(path)->setPen(pen);

        QPolygonF poly;
        poly.append(QPointF(toP.x() - 4, toP.y()));
        poly.append(QPointF(toP.x() + 4, toP.y()));
        poly.append(QPointF(toP.x(), toP.y() - 12));

        QGraphicsItem *arrow = scene->addPolygon(poly, pen, QBrush(Qt::black));
        arrow->setTransformOriginPoint(toP.x(), toP.y());
        arrow->setRotation(-path.angleAtPercent(1) + 90);
    }




}


#if 0
        if (directive == QLatin1String("graph")) {
            qreal scale, width, height;
            in >> scale >> width >> height;
            scene->setSceneRect(0, 0, width * SCALE, height * SCALE);

        } else if (directive == QLatin1String("artifact")) {
            QString name, label, style, shape, color, fillcolor;
            qreal x, y, width, height;
            in >> name >> x >> y >> width >> height;

            unsigned char labelReadState = 0;
            while (!in.atEnd() && labelReadState != 2) {
                QString s = in.read(1);
                switch (labelReadState) {
                case 0:
                    if (s.at(0) == QLatin1Char('"'))
                        labelReadState = 1;
                    break;
                case 1:
                    if (s == QLatin1String("\"")) {
                        labelReadState = 2;
                    } else {
                        label.append(s);
                    }
                    break;
                }
            }
            in >> style >> shape >> color >> fillcolor;

            label.replace(QLatin1String("\\n"), QLatin1String("\n"));
            QPen pen = QColor(color);
            QBrush brush = QColor(fillcolor);

            QRectF rc;
            rc.setSize(QSizeF(width * SCALE, height * SCALE));
            rc.moveCenter(QPointF(x * SCALE, toY(y * SCALE)));

            QGraphicsItem *item = 0;
            if (shape == QLatin1String("ellipse"))
                item = scene->addEllipse(rc, pen, brush);
            else
                item = scene->addRect(rc, pen, brush);
            item->setToolTip(label);

            QGraphicsSimpleTextItem *slabel = scene->addSimpleText(label);
            slabel->setBrush(Qt::black);
            slabel->setFont(sceneFont);

            QRect textrect = QStyle::alignedRect (Qt::LeftToRight,
                    Qt::AlignCenter,
                    slabel->boundingRect().size().toSize(),
                    rc.toRect());
            slabel->setPos(textrect.topLeft());

        } else if (directive == QLatin1String("edge")) {
            QString tail, head;
            int n;
            QString color;

            in >> tail >> head >> n;
            color = line.split(' ').last();

            QVector<QPointF> points;
            for (int i = 0; i < n; ++i) {
                qreal x, y;
                in >> x >> y;
                points.append(QPointF(x * SCALE, toY(y * SCALE)));
            }

            QPainterPath path = fromControlPoints(points);

            QPen pen;
            pen.setWidth(2);
            if (color != "black") {
                pen = QPen(QColor(0x33,0x33,0xff));
                pen.setWidth(1);
            }

            scene->addPath(path)->setPen(pen);

            QPointF endPt = points.last();

            QPolygonF poly;
            poly.append(QPointF(endPt.x() - 4, endPt.y()));
            poly.append(QPointF(endPt.x() + 4, endPt.y()));
            poly.append(QPointF(endPt.x(), endPt.y() - 12));

            QGraphicsItem *arrow = scene->addPolygon(poly, pen, QBrush(color
                        == "black"? Qt::black : QColor(0x33,0x33,0xff)));
            arrow->setTransformOriginPoint(endPt.x(), endPt.y());
            arrow->setRotation(-path.angleAtPercent(1) + 90);

#endif


#include "graph.moc"
