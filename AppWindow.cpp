#include <iostream>

#include "AppWindow.h"



AppWindow::AppWindow(const char* progName, QWidget* parent)
: QMainWindow(parent)
, _progName(progName)
{
    const char * url = "rtsp://192.168.8.107:8554/live";

    this->setupUi();
    this->_rtspSession = new RtspSession(1024, this->_progName.c_str(), url, this);
    this->bindEvents();
}

void AppWindow::setupUi()
{
    QGridLayout *layout = new QGridLayout;

    this->_cameraScreen = new QLabel("Video 0", this);
    //this->_cameraScreen->setg
    this->_cameraScreen->setStyleSheet("background:'black';color:'white'");

    this->_startCaptureButton = new QPushButton(this);
    this->_startCaptureButton->setText("Start");

    this->_stopCaptureButton = new QPushButton(this);
    this->_stopCaptureButton->setText("Stop");

    layout->addWidget(this->_startCaptureButton, 0, 0);
    layout->addWidget(this->_stopCaptureButton, 1, 0);
    layout->addWidget(this->_cameraScreen, 2, 0);

    QWidget * central = new QWidget(this);

    setCentralWidget(central);
    centralWidget()->setLayout(layout);

    setWindowTitle("Camera Window");
    setFixedSize(800, 600);
}

void AppWindow::bindEvents()
{
    connect(this->_startCaptureButton, SIGNAL(clicked()), this, SLOT(onStart()));
    connect(this->_stopCaptureButton, SIGNAL(clicked()), this, SLOT(onStop()));
    connect(this->_rtspSession, SIGNAL(gotFrame(QImage)), this, SLOT(updateFrame(QImage)));
}

void AppWindow::onStart()
{
    std::cout << "starting..." << std::endl;
    this->_rtspSession->start();
}

void AppWindow::onStop()
{
    std::cout << "stoping..." << std::endl;
    this->_rtspSession->stop();
}

void AppWindow::updateFrame(const QImage &image)
{
    QPixmap pixmap = QPixmap::fromImage(image.scaled(this->_cameraScreen->size(), Qt::KeepAspectRatio));
    this->_cameraScreen->setPixmap(pixmap);
}

