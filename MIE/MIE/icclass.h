#ifndef ICCLASS_H
#define ICCLASS_H

#include <QObject>
#include <QString>

class icClass:public QObject
{
    Q_OBJECT
public:
    icClass(QString name);
public slots:
    void doWork(QString filePath);
signals:
    void send(int);
private:
    QString name;
};

#endif // ICCLASS_H
