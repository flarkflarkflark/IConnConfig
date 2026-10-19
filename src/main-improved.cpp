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

	// Auto-detect desktop environment and apply appropriate styling
	QString desktop = qgetenv("XDG_CURRENT_DESKTOP").toLower();
	QString session = qgetenv("DESKTOP_SESSION").toLower();
	
	if (desktop.contains("kde") || desktop.contains("plasma")) {
		// KDE Plasma - use native Breeze style
		app.setStyle("Breeze");
	} else if (desktop.contains("gnome") || desktop.contains("unity")) {
		// GNOME - use Adwaita-like styling
		app.setStyle("Fusion");
		// Apply GNOME-like palette
		QPalette palette;
		palette.setColor(QPalette::Window, QColor(240, 240, 240));
		palette.setColor(QPalette::WindowText, QColor(0, 0, 0));
		palette.setColor(QPalette::Base, QColor(255, 255, 255));
		palette.setColor(QPalette::AlternateBase, QColor(245, 245, 245));
		palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 220));
		palette.setColor(QPalette::ToolTipText, QColor(0, 0, 0));
		palette.setColor(QPalette::Text, QColor(0, 0, 0));
		palette.setColor(QPalette::Button, QColor(240, 240, 240));
		palette.setColor(QPalette::ButtonText, QColor(0, 0, 0));
		palette.setColor(QPalette::BrightText, QColor(255, 0, 0));
		palette.setColor(QPalette::Link, QColor(0, 0, 255));
		palette.setColor(QPalette::Highlight, QColor(76, 163, 224));
		palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
		app.setPalette(palette);
	} else {
		// Fallback - use system default or Fusion
		QStringList availableStyles = QStyleFactory::keys();
		if (availableStyles.contains("Fusion")) {
			app.setStyle("Fusion");
		}
	}

	// Apply custom stylesheet for better visibility
	QString styleSheet = R"(
		QMainWindow {
			font-size: 10pt;
		}
		QDockWidget {
			font-size: 10pt;
			font-weight: bold;
		}
		QDockWidget::title {
			background: palette(highlight);
			color: palette(highlighted-text);
			padding: 4px;
			border-radius: 3px;
		}
		QTabWidget::pane {
			border: 1px solid palette(mid);
			border-radius: 3px;
		}
		QTabBar::tab {
			padding: 6px 12px;
			margin: 2px;
		}
		QTabBar::tab:selected {
			background: palette(highlight);
			color: palette(highlighted-text);
		}
		QPushButton {
			padding: 6px 12px;
			border-radius: 3px;
			border: 1px solid palette(mid);
		}
		QPushButton:hover {
			background: palette(highlight);
			color: palette(highlighted-text);
		}
		QGroupBox {
			font-weight: bold;
			border: 2px solid palette(mid);
			border-radius: 5px;
			margin: 10px 0px;
			padding-top: 10px;
		}
		QGroupBox::title {
			subcontrol-origin: margin;
			left: 10px;
			padding: 0 5px 0 5px;
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

	// Add scaling option for high-DPI displays
	QCommandLineOption scaleOption(
		QStringList() << "s" << "scale",
		QCoreApplication::translate("main", "UI scale factor (1.0-3.0)."),
		"scale");
	parser->addOption(scaleOption);

	parser->process(app);

	// Handle scaling
	if (parser->isSet(scaleOption)) {
		bool ok;
		double scale = parser->value(scaleOption).toDouble(&ok);
		if (ok && scale >= 1.0 && scale <= 3.0) {
			app.setAttribute(Qt::AA_EnableHighDpiScaling);
			qputenv("QT_SCALE_FACTOR", QByteArray::number(scale));
		}
	}

	MioMain w(parser);
	w.show();
	return app.exec();
}