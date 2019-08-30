#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDirIterator>
#include <thread>
#include <QThread>
#include <mutex>
#include "linuxprivate/qzipreader_p.h"
#include "linuxprivate/qzipwriter_p.h"
#include <QtGui>


static std::mutex g_lock;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->progressBar->setVisible(false);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_2_clicked()
{
   ui->lineEdit->setText(QFileDialog::getOpenFileName(this,
                                QString::fromUtf8("Open file"),
                                QDir::currentPath(),
                                "Minecraft Textures (*.zip)"));

}

void DeleteAllFiles(const QString &path)
{
    QDir oDir(path);
    QStringList files = oDir.entryList(QDir::Files);

    QStringList::Iterator itFile = files.begin();
    while (itFile != files.end())
    {
        QFile oFile(path + "/" + *itFile);
        oFile.remove();
        ++itFile;
    }

    QStringList dirs = oDir.entryList(QDir::Dirs);
    QStringList::Iterator itDir = dirs.begin();
    while (itDir != dirs.end())
    {
        if (*itDir != "." && *itDir != "..") DeleteAllFiles(path + "/" + *itDir);
        ++itDir;
    }
    oDir.rmdir(path);
}

void ImageConverter(QString path, int standartResolution, QString MTEPathTMP)
{
    QImage img(path);
    if ((img.width() == img.height())&&(img.width() == standartResolution)){
       // Сохранение в ZIP
       QFileInfo fileI(path);
       QString fileName = fileI.baseName();
       g_lock.lock();
       while (QFile(MTEPathTMP+"/ZIP/"+fileName+".png").exists())
           fileName += "Z";
       QFile file(MTEPathTMP+"/ZIP/"+fileName+".png");
       file.open(QIODevice::WriteOnly);
       g_lock.unlock();
       img.save(&file, "PNG");
       file.close();
    }
}

void MainWindow::on_pushButton_clicked()
{
    // Переменная ошибки
    bool exit = false;

    // Визуальный переход
    ui->lineEdit->setReadOnly(true);
    ui->pushButton->setEnabled(false);
    ui->progressBar->setVisible(true);
    ui->progressBar->setValue(5);
    ui->pushButton_2->setEnabled(false);

    // Открываем файл
    QZipReader zip_reader(ui->lineEdit->text());
    if (!exit){
        if (!zip_reader.exists()){
            QMessageBox::warning(this, "Warning","Can't open ZIP file!");
            exit = true;
        }
    }
    ui->progressBar->setValue(10);

    // Создаем временную папку
    QString MTEPathTMP = QDir::tempPath()+"/MTE99";
    if (!exit){
         QDir dir;
         if((!dir.mkpath(MTEPathTMP)) || (!dir.mkpath(MTEPathTMP+"/ZIP")||(!dir.mkpath(MTEPathTMP+"/SRC")))){
             QMessageBox::warning(this, "Warning","Can't create temp folder!");
             exit = true;
         }
    }
    ui->progressBar->setValue(15);

    // Распаковываем в неё файлы
    if (!exit){
        zip_reader.extractAll(MTEPathTMP+"/SRC");
        if (zip_reader.status() != QZipReader::NoError){
            QMessageBox::warning(this, "Warning","Can't unpack ZIP file!");
            exit = true;
        }
    }
    ui->progressBar->setValue(30);

    // Получение списка файлов в папке
    QStringList nameFilter;
    if (!exit){
        QDirIterator it(MTEPathTMP+"/SRC", QStringList() << "*.png", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
            nameFilter << it.next();
    }
    ui->progressBar->setValue(35);

    // Анализ изображений
    if (!exit){
        QImage img;
        // Инициализация стандартного разрешения
        int standartResolution = 0;
        for (int i = 0; (i < nameFilter.size())&& (standartResolution == 0); i++ ){
            QFileInfo fileI(nameFilter.at(i));
             if (fileI.baseName() == "empty_armor_slot_boots"){
                 img.load(nameFilter.at(i));
                 standartResolution = img.width();
             }
        }

        // Запуск потоков
        std::list<std::thread> threadList;
        int maxThreads = QThread::idealThreadCount();
        for (int i = 0; i < nameFilter.size(); ){
            int j = 0;
            for (; i<nameFilter.size() && j < maxThreads; j++){
                threadList.push_back(std::thread(ImageConverter, nameFilter.at(i), standartResolution, MTEPathTMP));
                i++;           }

            for (; j > 0; j-- ){
               if (threadList.begin()->joinable()) threadList.begin()->join();
                threadList.pop_front();
            }


        }
    }
    ui->progressBar->setValue(50);

    // Выбор места сохранения
     QString outpt;
     if (!exit){
         outpt = (QFileDialog::getSaveFileName(this,
                                     QString::fromUtf8("Save file"),
                                     QDir::currentPath(),
                                     "Minecraft Textures (*.zip)"));
     }
     ui->progressBar->setValue(55);

    // Создание ZIP файла
    QZipWriter zip(outpt);
    if (!exit){
        zip.setCompressionPolicy(QZipWriter::AutoCompress);
        if (zip.status() != QZipWriter::NoError){
            QMessageBox::warning(this, "Warning","Can't save ZIP file!");
            exit = true;
        }  
    }
    ui->progressBar->setValue(60);

    // Архивация
    if (!exit){
        QDirIterator it(MTEPathTMP+"/ZIP/", QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString file_path = it.next();
            if (it.fileInfo().isDir()) {
                zip.setCreationPermissions(QFile::permissions(file_path));
                zip.addDirectory(file_path.remove(MTEPathTMP+"/ZIP/"));
            } else if (it.fileInfo().isFile()) {
                QFile file(file_path);
                if (!file.open(QIODevice::ReadOnly)) continue;
                zip.setCreationPermissions(QFile::permissions(file_path));
                QByteArray ba = file.readAll();
                zip.addFile(file_path.remove(MTEPathTMP+"/ZIP/"), ba);
            }
        }
        zip.close();
    }
    ui->progressBar->setValue(80);

    // Чистка за собой
    DeleteAllFiles(MTEPathTMP);
    ui->progressBar->setValue(100);

    // Визуальный переход
    ui->lineEdit->setReadOnly(false);
    ui->pushButton->setEnabled(true);
    ui->progressBar->setVisible(false);
    ui->progressBar->setValue(0);
    ui->pushButton_2->setEnabled(true);
}
