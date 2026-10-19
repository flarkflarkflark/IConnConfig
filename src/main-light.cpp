#include "iconnconfigmain.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>
#include <QStyleFactory>
#include <QDir>

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);
	QCoreApplication::setOrganizationName("punkt-k");
	QCoreApplication::setOrganizationDomain("www.punkt-k.de");
	QCoreApplication::setApplicationName("iConnConfig");
	QCoreApplication::setApplicationVersion("0.5-beta");

	// Force light theme and better styling
	app.setStyle("Fusion");
	
	// Light theme palette
	QPalette lightPalette;
	lightPalette.setColor(QPalette::Window, QColor(240, 240, 240));
	lightPalette.setColor(QPalette::WindowText, QColor(0, 0, 0));
	lightPalette.setColor(QPalette::Base, QColor(255, 255, 255));
	lightPalette.setColor(QPalette::AlternateBase, QColor(245, 245, 245));
	lightPalette.setColor(QPalette::ToolTipBase, QColor(255, 255, 220));
	lightPalette.setColor(QPalette::ToolTipText, QColor(0, 0, 0));
	lightPalette.setColor(QPalette::Text, QColor(0, 0, 0));
	lightPalette.setColor(QPalette::Button, QColor(225, 225, 225));
	lightPalette.setColor(QPalette::ButtonText, QColor(0, 0, 0));
	lightPalette.setColor(QPalette::BrightText, QColor(255, 0, 0));
	lightPalette.setColor(QPalette::Link, QColor(42, 130, 218));
	lightPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
	lightPalette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
	lightPalette.setColor(QPalette::Mid, QColor(200, 200, 200));
	lightPalette.setColor(QPalette::Dark, QColor(180, 180, 180));
	app.setPalette(lightPalette);

	// Comprehensive stylesheet for better visibility
	QString styleSheet = R"(
		QMainWindow {
			background-color: #f0f0f0;
			font-size: 11pt;
		}
		
		QDockWidget {
			background-color: #f8f8f8;
			border: 1px solid #c0c0c0;
			font-size: 11pt;
			font-weight: bold;
			min-width: 250px;
		}
		
		QDockWidget::title {
			background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
				stop:0 #e8e8e8, stop:1 #d0d0d0);
			color: #000000;
			padding: 8px;
			border: 1px solid #a0a0a0;
			border-bottom: none;
			font-weight: bold;
			font-size: 11pt;
		}
		
		QDockWidget::close-button, QDockWidget::float-button {
			background: #d0d0d0;
			border: 1px solid #a0a0a0;
			border-radius: 3px;
			padding: 2px;
		}
		
		QDockWidget::close-button:hover, QDockWidget::float-button:hover {
			background: #2a82da;
		}
		
		QTabWidget::pane {
			border: 2px solid #c0c0c0;
			background-color: #ffffff;
			border-radius: 4px;
		}
		
		QTabBar::tab {
			background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
				stop:0 #e8e8e8, stop:1 #d0d0d0);
			border: 1px solid #a0a0a0;
			padding: 8px 16px;
			margin: 2px;
			border-radius: 4px;
			font-size: 11pt;
			font-weight: bold;
			min-width: 100px;
		}
		
		QTabBar::tab:selected {
			background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
				stop:0 #2a82da, stop:1 #1e5f99);
			color: white;
			border-color: #1e5f99;
		}
		
		QTabBar::tab:hover:!selected {
			background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
				stop:0 #f0f0f0, stop:1 #e0e0e0);
		}
		
		QPushButton {
			background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
				stop:0 #e8e8e8, stop:1 #d0d0d0);
			border: 1px solid #a0a0a0;
			padding: 8px 16px;
			border-radius: 4px;
			font-size: 11pt;
			font-weight: bold;
			min-height: 20px;
		}
		
		QPushButton:hover {
			background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
				stop:0 #3498db, stop:1 #2980b9);
			color: white;
			border-color: #2980b9;
		}
		
		QPushButton:pressed {
			background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
				stop:0 #2980b9, stop:1 #3498db);
		}
		
		QGroupBox {
			font-weight: bold;
			font-size: 12pt;
			border: 2px solid #a0a0a0;
			border-radius: 6px;
			margin: 15px 0px;
			padding-top: 15px;
			background-color: #f8f8f8;
		}
		
		QGroupBox::title {
			subcontrol-origin: margin;
			left: 15px;
			padding: 0 8px 0 8px;
			background-color: #f0f0f0;
			border: 1px solid #a0a0a0;
			border-radius: 3px;
		}
		
		QLabel {
			font-size: 10pt;
			color: #000000;
		}
		
		QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
			border: 1px solid #a0a0a0;
			border-radius: 3px;
			padding: 6px;
			background-color: white;
			font-size: 10pt;
			min-height: 20px;
		}
		
		QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
			border: 2px solid #2a82da;
		}
		
		QTableWidget, QTreeWidget, QListWidget {
			border: 1px solid #a0a0a0;
			background-color: white;
			alternate-background-color: #f5f5f5;
			font-size: 10pt;
			gridline-color: #d0d0d0;
		}
		
		QTableWidget::item, QTreeWidget::item, QListWidget::item {
			padding: 4px;
			border-bottom: 1px solid #e0e0e0;
		}
		
		QTableWidget::item:selected, QTreeWidget::item:selected, QListWidget::item:selected {
			background-color: #2a82da;
			color: white;
		}
		
		QHeaderView::section {
			background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
				stop:0 #e8e8e8, stop:1 #d0d0d0);
			border: 1px solid #a0a0a0;
			padding: 6px;
			font-weight: bold;
			font-size: 10pt;
		}
		
		QScrollBar:vertical {
			background: #f0f0f0;
			width: 16px;
			border: 1px solid #c0c0c0;
			border-radius: 8px;
		}
		
		QScrollBar::handle:vertical {
			background: #c0c0c0;
			border-radius: 6px;
			min-height: 20px;
		}
		
		QScrollBar::handle:vertical:hover {
			background: #a0a0a0;
		}
		
		QScrollBar:horizontal {
			background: #f0f0f0;
			height: 16px;
			border: 1px solid #c0c0c0;
			border-radius: 8px;
		}
		
		QScrollBar::handle:horizontal {
			background: #c0c0c0;
			border-radius: 6px;
			min-width: 20px;
		}
		
		QScrollBar::handle:horizontal:hover {
			background: #a0a0a0;
		}
		
		QToolBar {
			background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
				stop:0 #f0f0f0, stop:1 #e0e0e0);
			border: 1px solid #c0c0c0;
			padding: 4px;
			spacing: 4px;
		}
		
		QToolButton {
			background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
				stop:0 #e8e8e8, stop:1 #d0d0d0);
			border: 1px solid #a0a0a0;
			border-radius: 4px;
			padding: 6px;
			font-size: 10pt;
			min-width: 60px;
			min-height: 30px;
		}
		
		QToolButton:hover {
			background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
				stop:0 #3498db, stop:1 #2980b9);
			color: white;
		}
		
		QMenuBar {
			background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
				stop:0 #f0f0f0, stop:1 #e0e0e0);
			border-bottom: 1px solid #c0c0c0;
			font-size: 11pt;
		}
		
		QMenuBar::item {
			padding: 6px 12px;
			background: transparent;
		}
		
		QMenuBar::item:selected {
			background: #2a82da;
			color: white;
		}
		
		QMenu {
			background-color: white;
			border: 1px solid #c0c0c0;
			font-size: 10pt;
		}
		
		QMenu::item {
			padding: 6px 20px;
		}
		
		QMenu::item:selected {
			background-color: #2a82da;
			color: white;
		}
		
		QStatusBar {
			background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
				stop:0 #e0e0e0, stop:1 #d0d0d0);
			border-top: 1px solid #c0c0c0;
			font-size: 10pt;
		}
	)";
	
	app.setStyleSheet(styleSheet);

	QTranslator mioConfigTranslator;
	QTranslator qtTranslator;
	QLocale::setDefault(QLocale("en_EN"));

	if (mioConfigTranslator.load(QLocale(), QLatin1String("iconnconfig"),
								 QLatin1String("_"),
								 QLatin1String(":/translations/tr"))) {
		app.installTranslator(&mioConfigTranslator);

		qtTranslator.load(
			QLocale(), QLatin1String("qt"), QLatin1String("_"),
			QLibraryInfo::location(QLibraryInfo::TranslationsPath));
		app.installTranslator(&qtTranslator);
	}

	QCommandLineParser *parser = new QCommandLineParser();
	parser->setApplicationDescription("iConnConfig - iConnectivity Device Configuration");
	parser->addHelpOption();
	parser->addVersionOption();

	QCommandLineOption filenameOption(
		QStringList() << "f" << "filename",
		QCoreApplication::translate("main", "filename to read settings from."),
		"filename");
	parser->addOption(filenameOption);

	parser->process(app);

	MioMain w(parser);
	w.show();
	return app.exec();
}